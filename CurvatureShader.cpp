#include "CurvatureShader.h"

#include <maya\MGlobal.h>
#include <set>
#include <maya\MTransformationMatrix.h>

// Attributes
MObject			 CurvatureShader::aColorMap;
MObject			 CurvatureShader::aFlatShading;
MObject			 CurvatureShader::aScale;
MCallbackIdArray CurvatureShader::callbacks;
const MTypeId	 CurvatureShader::typeId(0x00127883);

CurvatureShader::CurvatureShader(){}

CurvatureShader::~CurvatureShader(){
	for (auto &ptr : m_data)
		delete ptr.second;
}

void* CurvatureShader::creator(){
	return new CurvatureShader();
}

void CurvatureShader::postConstructor() {
	setMPSafe(true);
	setColorMap();
}

MStatus CurvatureShader::initialize(){
	MStatus status(MStatus::kSuccess);

	MFnNumericAttribute nAttr;
	MRampAttribute rAttr;
	MFnEnumAttribute eAttr;

	aColorMap = rAttr.createColorRamp("curvatureMap", "cm", &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = addAttribute(aColorMap);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	attributeAffects(aColorMap, outColor);

	aFlatShading = nAttr.create("flatShading", "fs", MFnNumericData::kBoolean, 1, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = addAttribute(aFlatShading);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	attributeAffects(aFlatShading, outColor);

	aScale = nAttr.create("scaleFactor", "sf", MFnNumericData::kDouble, 5.0, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	nAttr.setMin(0.0);
	nAttr.setSoftMax(100.0);
	status = addAttribute(aScale);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	attributeAffects(aScale, outColor);

	return status;
}

MStatus CurvatureShader::glBind(const MDagPath &shapePath) {
	
	if (!shapePath.hasFn(MFn::kMesh))
		return MStatus::kFailure;
	
	CurvatureShaderData* data = getDataPtr(const_cast<MDagPath&>(shapePath));

	if (NULL == data) {
		std::string path = shapePath.fullPathName().asChar();
		m_data[path] = new CurvatureShaderData(const_cast<MDagPath&>(shapePath));
	}

	return MS::kSuccess;
}

MStatus CurvatureShader::glGeometry(const MDagPath &shapePath,
	int prim,
	unsigned int writable,
	int indexCount,
	const unsigned int *indexArray,
	int vertexCount,
	const int *vertexIDs,
	const float *vertexArray,
	int normalCount,
	const float** normalArrays,
	int colorCount,
	const float** colorArrays,
	int texCoordCount,
	const float** texCoordArrays)
{
	MStatus status(MStatus::kSuccess);

	if (!shapePath.hasFn(MFn::kMesh))
		return MStatus::kFailure;

	// Trigger compute
	MFnDependencyNode fnNode(thisMObject());
	MPlug pOutColor = fnNode.findPlug(outColor, &status);
	CHECK_MSTATUS(status);
	pOutColor.asMObject();

	CurvatureShaderData* data = getDataPtr(const_cast<MDagPath&>(shapePath));

	if (NULL == data)
		return MS::kFailure;

	// Update values
	if (data->vp2 == true) {
		data->dirtyNode = true;
		data->vp2 = false;
	}

	status = updateCurvature(indexCount, indexArray, vertexCount, vertexArray, vertexIDs, normalArrays[0], shapePath.inclusiveMatrix(), data);
	CHECK_MSTATUS_AND_RETURN_IT(status);

	float *colors = new float[vertexCount*3];
	data->getColors(vertexIDs, vertexCount, colors);

	// Draw mesh
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);

	if (m_flatShading)
		glDisable(GL_LIGHTING);
	else {
		glEnable(GL_LIGHTING);
		glEnable(GL_COLOR_MATERIAL);
		glColorMaterial(GL_FRONT, GL_DIFFUSE);
	}

	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, &vertexArray[0]);
	glEnableClientState(GL_COLOR_ARRAY);
	glColorPointer(3, GL_FLOAT, 0, colors);
	glEnableClientState(GL_NORMAL_ARRAY);
	glNormalPointer(GL_FLOAT, 0, &normalArrays[0][0]);

	glDrawElements(prim, indexCount, GL_UNSIGNED_INT, indexArray);

	glPopClientAttrib();
	glPopAttrib();
	
	delete[] colors;
	colors = NULL;

	return MS::kSuccess;
}

MStatus CurvatureShader::updateCurvature(int indexCount, const unsigned int *indexArray, int vertexCount, const float *vertexArray, const int *vertexIDs, const float *normalArray, MMatrix &transform, CurvatureShaderData *data) {
	MStatus status;
	
	// Update vertex curvature ///////////////////////////////////////////////////////////////////////
	if (data->dirtyNode) {
		data->dirtyNode = false;
		data->dirtyColor = true;

		MTransformationMatrix tMatrix(transform);
		transform = tMatrix.asScaleMatrix();

		std::set <std::string> combinations;
		std::map <unsigned int, MPoint> vertices;
		std::map <unsigned int, MVector> normals;
		std::map <unsigned int, bool> dirty;
		std::map <unsigned int, double> curvature;
		std::map <unsigned int, unsigned int> valence;

		// Map vertex and normal to indices
		for (int i = 0; i < vertexCount; ++i) {
			int vtxId = vertexIDs[i];

			if (vertices.find(vtxId) == vertices.end()) {
				const float* vertex = &vertexArray[i * 3];
				vertices[vtxId] = vertex;
				vertices[vtxId] *= transform;

				const float* normal = &normalArray[i * 3];
				normals[vtxId] = normal;

				curvature[vtxId] = 0;
				valence[vtxId] = 0;
			}
			else {
				const float* normal = &normalArray[i * 3];
				normals[vtxId] += normal;
			}
		}

		// Average normal
		for (auto &normal : normals) {
			normal.second *= transform;
			normal.second.normalize();

			int vtxId = normal.first;
			
			// Check if vertex changed
			if (vertices[vtxId] == data->vertices[vtxId] && normals[vtxId] == data->normals[vtxId]) {
				curvature[vtxId] = data->curvature[vtxId];
				dirty[vtxId] = false;
			}
			else
				dirty[vtxId] = true;
		}

		// Iterate over triangles
		for (int i = 0; i < indexCount; i += 3) {
			
			// Iterate over vertices in a triangle
			for (unsigned int t = 0; t < 3; t++) {
				if (vertexCount <= (int)indexArray[i + t])
					return MS::kSuccess;

				unsigned int idA = vertexIDs[indexArray[i + t]];

				if (!dirty[idA])
					continue;

				// Iterate over connected edges
				for (unsigned int v = 1; v <= 2; v++) {
					unsigned int idB = vertexIDs[indexArray[i + ((t + v) % 3)]];

					char buffer[16];
					sprintf_s(buffer, "%d;%d", idA, idB);
					if (combinations.find(buffer) != combinations.end())
						continue;
					combinations.insert(buffer);

					// Compute vertex curvature
					MVector edge(vertices[idB].x - vertices[idA].x, vertices[idB].y - vertices[idA].y, vertices[idB].z - vertices[idA].z);
					double angle = acos(normals[idA] * edge.normal());

					double c = 0;
					if (angle != M_PI / 2) {
						double compAngle = (angle < M_PI / 2) ? angle : (M_PI - angle);
						c = 1 / (edge.length() / 2 * sin(compAngle) / sin(M_PI / 2 - compAngle));
						if (angle < M_PI / 2)
							c *= -1;
					}

					curvature[idA] += c;
					valence[idA]++;
				}
			}
		}

		// Calculate average curvatire
		for (auto &vtxCrv : curvature)
			if (1 < valence[vtxCrv.first] && dirty[vtxCrv.first])
				vtxCrv.second /= valence[vtxCrv.first];

		data->vertices = vertices;
		data->normals = normals;
		data->curvature = curvature;
	}

	// Update vertex color ////////////////////////////////////////////////////////////////////////
	if (data->dirtyColor) {
		data->dirtyColor = false;

		status = updateColors(data);
		CHECK_MSTATUS_AND_RETURN_IT(status);
	}

	return MS::kSuccess;
}

MStatus CurvatureShader::updateColors(CurvatureShaderData *data){
	MStatus status;

	data->color.clear();

	MRampAttribute map(thisMObject(), aColorMap, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);

	for (auto &curvatue : data->curvature){
		double value = curvatue.second * m_scale + 0.5;
		map.getColorAtPosition(float(value), data->color[curvatue.first], &status);
		CHECK_MSTATUS_AND_RETURN_IT(status);
	}

	return MS::kSuccess;
}

MStatus CurvatureShader::setDependentsDirty(const MPlug& plug, MPlugArray& plugArray){
	if (plug == aScale)
		m_dirtyScale = true;

	MPlug dirtyPlug(plug);
	if (dirtyPlug.isChild())
		dirtyPlug = dirtyPlug.parent();
	if (dirtyPlug.isElement())
		dirtyPlug = dirtyPlug.array();
	if (dirtyPlug == aColorMap)
		m_dirtyMap = true;

	if (plug == aFlatShading)
		m_dirtyShading = true;

	return MS::kSuccess;
}

MStatus CurvatureShader::setColorMap(){
	MStatus status(MStatus::kSuccess);

	MRampAttribute map(thisMObject(), aColorMap, &status);
	CHECK_MSTATUS(status);

	MColorArray col;

	col.append(0.0f, 0.0f, 1.0f);
	col.append(0.0f, 1.0f, 0.0f);
	col.append(1.0f, 0.0f, 0.0f);

	MFloatArray pos;
	MIntArray itp;
	for (unsigned i = 0; i < col.length(); i++) {
		pos.append(float(i) * 1.0f / (col.length() - 1));
		itp.append(MRampAttribute::kLinear);
	}

	status = map.setRamp(col, pos, itp);
	CHECK_MSTATUS_AND_RETURN_IT(status);

	return status;
}

void CurvatureShader::dirtyAll() {
	for (auto &data : m_data)
		data.second->dirtyColor = true;
}

CurvatureShaderData* CurvatureShader::getDataPtr(MDagPath& path) {
	std::string strPath = path.fullPathName().asChar();

	if (m_data.find(strPath) == m_data.end())
		return NULL;

	return m_data[strPath];
}

MStatus CurvatureShader::compute(const MPlug& plug, MDataBlock& datablock){
	MStatus status(MStatus::kSuccess);

	if (m_dirtyScale) {
		m_dirtyScale = false;
		m_scale = datablock.inputValue(aScale).asDouble();
		dirtyAll();
	}

	if (m_dirtyMap) {
		m_dirtyMap = false;
		dirtyAll();
	}

	if (m_dirtyShading) {
		m_dirtyShading = false;
		m_flatShading = datablock.inputValue(aFlatShading).asBool();
	}

	datablock.outputValue(outColor).setClean();

	return status;
}

void CurvatureShader::preConnection(MPlug &srcPlug, MPlug &destPlug, bool made, void *clientData) {

	if (made ||
		destPlug.node().apiType() != MFn::kShadingEngine ||
		srcPlug.node().apiType()!=MFn::kMesh)
		return;

	MFnDagNode fnNode(srcPlug.node());

	MObject aOg = fnNode.findPlug("og").attribute();
	if (srcPlug.attribute() == aOg)
		srcPlug = srcPlug.array().parent();

	MObject aIog = fnNode.findPlug("iog").attribute();
	if (srcPlug.attribute() != aIog)
		return;

	MDagPathArray allPaths;
	fnNode.getAllPaths(allPaths);

	MDagPath path = allPaths[srcPlug.logicalIndex()];
	std::string strPath = path.fullPathName().asChar();

	MItDependencyGraph itGraph(destPlug.node(), MFn::kPluginHwShaderNode, MItDependencyGraph::kUpstream);
	for (itGraph.reset(); !itGraph.isDone(); itGraph.next()) {

		MFnDependencyNode fnShNode(itGraph.currentItem());
		if (CurvatureShader::typeId != fnShNode.typeId())
			continue;

		CurvatureShader *shaderPtr = dynamic_cast <CurvatureShader*>(fnShNode.userNode());

		delete shaderPtr->m_data[strPath];
		shaderPtr->m_data.erase(strPath);
	}
}

void CurvatureShader::nodeDirty(MObject& node, MPlug& plug, void *clientData) {
	CurvatureShaderData *data = (CurvatureShaderData*)clientData;
	data->dirtyNode = true;
}

void CurvatureShader::transformDirty(MObject& node, MDagMessage::MatrixModifiedFlags &modified, void *clientData) {
	CurvatureShaderData *data = (CurvatureShaderData*)clientData;
	data->dirtyNode = true;
}