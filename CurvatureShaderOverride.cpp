#include "CurvatureShaderOverride.h"

#include <maya\MDrawContext.h>
#include <maya\MGlobal.h>
#include <maya/MGLFunctionTable.h>

MString CurvatureShaderOverride::registrantId = "curvatureShaderRegistrantId";

MString CurvatureShaderOverride::initialize(const MInitContext &initContext, MInitFeedback &initFeedback){
	addIndexingRequirement(MHWRender::MIndexBufferDescriptor(MHWRender::MIndexBufferDescriptor::kTriangle, "indices", MHWRender::MGeometry::kTriangles, 0, MObject::kNullObj, MHWRender::MGeometry::kInt16));
	addGeometryRequirement(MHWRender::MVertexBufferDescriptor("positions", MHWRender::MGeometry::kPosition, MHWRender::MGeometry::kFloat, 3));
	addGeometryRequirement(MHWRender::MVertexBufferDescriptor("normals", MHWRender::MGeometry::kNormal, MHWRender::MGeometry::kFloat, 3));
	addGeometryRequirement(MHWRender::MVertexBufferDescriptor("color", MHWRender::MGeometry::kColor, MHWRender::MGeometry::kFloat, 3));

	MInitContext* context = const_cast<MInitContext*>(&initContext);

	fShaderNode = (CurvatureShader*)MPxHwShaderNode::getHwShaderNodePtr(context->shader);

	MDagPath nodePath = context->dagPath;

	std::string path = nodePath.fullPathName().asChar();

	CurvatureShaderData *data = fShaderNode->getDataPtr(nodePath);
	if (NULL==data)
		fShaderNode->m_data[path] = new CurvatureShaderData(nodePath);

	return "Autodesk Maya curvatureShader";
}


bool CurvatureShaderOverride::draw(MHWRender::MDrawContext& context, const MHWRender::MRenderItemList& renderItemList) const {
	MStatus status;

	// Trigger compute
	MFnDependencyNode fnNode(fShaderNode->thisMObject());
	MPlug pOutColor = fnNode.findPlug(CurvatureShader::outColor, &status);
	CHECK_MSTATUS(status);
	pOutColor.asMObject();
	
	// Update values
	for (int renderItemIdx = 0; renderItemIdx < renderItemList.length(); renderItemIdx++)
	{
		const MHWRender::MRenderItem* renderItem = renderItemList.itemAt(renderItemIdx);
		const MHWRender::MGeometry* geometry = renderItem->geometry();

		CurvatureShaderData* data = fShaderNode->getDataPtr(renderItem->sourceDagPath());
		if (NULL == data || !renderItem->sourceDagPath().hasFn(MFn::kMesh))
			continue;
		
		// Create index list (just for compatibility with legacy viewport)
		MHWRender::MVertexBuffer *clrBuffer = const_cast<MHWRender::MVertexBuffer*>(geometry->vertexBuffer(2));
		unsigned int numVertices = clrBuffer->vertexCount();
		int *vertexIDs = new int[numVertices];
		for (unsigned int i = 0; i < numVertices; i++)
			vertexIDs[i] = i;

		// Update curvature
		MHWRender::MIndexBuffer *idxBuffer = const_cast<MHWRender::MIndexBuffer*>(geometry->indexBuffer(0));
		MHWRender::MVertexBuffer *vtxBuffer = const_cast<MHWRender::MVertexBuffer*>(geometry->vertexBuffer(0));
		MHWRender::MVertexBuffer *nrmBuffer = const_cast<MHWRender::MVertexBuffer*>(geometry->vertexBuffer(1));

		unsigned int *indices = (unsigned int*)idxBuffer->map();
		float *vertexArray = (float*)vtxBuffer->map();
		float *normalArray = (float*)nrmBuffer->map();

		if (data->vp2 == false) {
			data->dirtyNode = true;
			data->vp2 = true;
		}

		status = fShaderNode->updateCurvature(
			idxBuffer->size(),
			indices,
			numVertices,
			vertexArray,
			vertexIDs,
			normalArray,
			context.getMatrix(MHWRender::MFrameContext::kWorldMtx),
			data);
		CHECK_MSTATUS_AND_RETURN_IT(status);

		idxBuffer->unmap();
		vtxBuffer->unmap();
		nrmBuffer->unmap();

		// Update vtx colors
		float *colors = (float*)clrBuffer->acquire(numVertices, true);
		data->getColors(vertexIDs, numVertices, colors);
		clrBuffer->commit(colors);
	}

	// Draw ///////////////////////////////////////////////////////////////////////////////////////
	// Set world view matrix
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	MMatrix transform = context.getMatrix(MHWRender::MFrameContext::kWorldViewMtx, &status);
	if (status)
		glLoadMatrixd(transform.matrix[0]);

	// Set projection matrix
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	MMatrix projection = context.getMatrix(MHWRender::MFrameContext::kProjectionMtx, &status);
	if (status)
		glLoadMatrixd(projection.matrix[0]);

	// Draw geometry
	drawGeometry(context);

	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	return true;
}