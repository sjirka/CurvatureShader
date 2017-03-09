#include "curvatureShader.h"
#include <maya\MGlobal.h>

CurvatureShaderData::CurvatureShaderData(MDagPath& path) : MUserData(false) {
	callbacks.append(MNodeMessage::addNodeDirtyPlugCallback(path.node(), CurvatureShader::nodeDirty, this));
	callbacks.append(MDagMessage::addWorldMatrixModifiedCallback(path, CurvatureShader::transformDirty, this));
}

CurvatureShaderData::~CurvatureShaderData() {
	MMessage::removeCallbacks(callbacks);
}

bool CurvatureShaderData::getColors(const int *vertexIDs, int vertexCount, float *colors) {
	for (int i = 0; i < vertexCount; i++) {
		unsigned int vtxId = vertexIDs[i];

		colors[i * 3 + 0] = color[vtxId].r;
		colors[i * 3 + 1] = color[vtxId].g;
		colors[i * 3 + 2] = color[vtxId].b;
	}

	return true;
}