#pragma once

#include <maya\MPxHwShaderNode.h>
#include <maya\MPlug.h>
#include <maya\MFnMesh.h>
#include <maya\MDataBlock.h>
#include <maya\MDataHandle.h>
#include <maya\MRampAttribute.h>
#include <maya\MColorArray.h>
#include <maya\MFnNumericAttribute.h>
#include <maya\MPointArray.h>
#include <maya\MFnMeshData.h>
#include <maya\MItMeshVertex.h>
#include <maya\MFnEnumAttribute.h>
#include <maya\MDoubleArray.h>
#include <maya\MCallbackIdArray.h>
#include <maya\MDGMessage.h>
#include <maya\MTimer.h>
#include <maya\MItDependencyGraph.h>
#include <maya\MNodeMessage.h>
#include <maya\MDagMessage.h>
#include <maya\MUserData.h>
#include <maya\MDagPathArray.h>
#include <maya\MMatrix.h>

#include <map>

class CurvatureShader;

class CurvatureShaderData : public MUserData{
public:
	CurvatureShaderData(MDagPath& path);
	virtual ~CurvatureShaderData();
	bool getColors(const int *vertexIDs, int vertexCount, float *colors);

	bool dirtyNode = true;
	bool dirtyColor = true;
	bool vp2 = false;

	std::map <unsigned int, MPoint> vertices;
	std::map <unsigned int, MVector> normals;

	std::map <unsigned int, double> curvature;
	std::map <unsigned int, MColor> color;

	MCallbackIdArray callbacks;
};

class CurvatureShader : public MPxHwShaderNode
{
public:
		CurvatureShader();
		virtual ~CurvatureShader();
		static void* creator();
		static MStatus initialize();
		
		virtual void postConstructor();

		virtual MStatus glBind(const MDagPath &shapePath);
		virtual MStatus glGeometry(const MDagPath &shapePath,
			int prim,
			unsigned int writable,
			int indexCount,
			const unsigned int *indexArray,
			int vertexCount,
			const int *vertexIDs,
			const float *vertexArray,
			int normalCount,
			const float ** normalArrays,
			int colorCount,
			const float ** colorArrays,
			int texCoordCount,
			const float ** texCoordArrays);
		virtual bool supportsBatching() const { return true; }
		virtual bool provideVertexIDs() { return true; }

		virtual MStatus setDependentsDirty(const MPlug& plug, MPlugArray& plugArray);
		virtual MStatus compute(const MPlug& plug, MDataBlock& block);
		MStatus updateCurvature(int indexCount,
			const unsigned int *indexArray,
			int vertexCount,
			const float *vertexArray,
			const int *vertexIDs,
			const float *normalArray,
			MMatrix &transform,
			CurvatureShaderData *data
			);
		MStatus updateColors(CurvatureShaderData *data);

		static void nodeDirty(MObject& node, MPlug& plug, void *clientData);
		static void transformDirty(MObject& node, MDagMessage::MatrixModifiedFlags &modified, void *clientData);

		void	dirtyAll();
		
		MStatus setColorMap(int preset);

		static void preConnection(MPlug &srcPlug, MPlug &destPlug, bool made, void *clientData);
		CurvatureShaderData* getDataPtr(MDagPath& path);

		static const MTypeId typeId;

		static MObject aColorMap;
		static MObject aFlatShading;
		static MObject aScale;
		static MObject aPreset;

		static MCallbackIdArray callbacks;

		bool m_flatShading;
		std::map <std::string, CurvatureShaderData*> m_data;

private:

	double m_scale;
	bool
		m_dirtyScale = true,
		m_dirtyPreset = true,
		m_dirtyMap = true,
		m_dirtyShading = true;
};