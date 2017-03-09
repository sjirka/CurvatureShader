#pragma once
#include "CurvatureShader.h"

#include <maya\MPxShaderOverride.h>

#include <maya\MHardwareRenderer.h>
#include <maya\MDrawRegistry.h>
#include <maya\MDrawContext.h>
#include <maya\MStateManager.h>
#include <maya\MGLdefinitions.h>
#include <maya\MHWGeometryUtilities.h>

class CurvatureShaderOverride : public MHWRender::MPxShaderOverride
{
public:
	static MHWRender::MPxShaderOverride* Creator(const MObject &obj) {return new CurvatureShaderOverride(obj);}

	virtual ~CurvatureShaderOverride() {fShaderNode = NULL;}

	virtual MString initialize(const MInitContext &initContext, MInitFeedback &initFeedback);
	virtual bool draw(MHWRender::MDrawContext& context, const MHWRender::MRenderItemList& renderItemList) const;
	virtual bool rebuildAlways() { return true; };

	virtual MHWRender::DrawAPI supportedDrawAPIs() const {
		return MHWRender::kOpenGL;
	}

	static MString registrantId;

protected:
	CurvatureShaderOverride(const MObject& obj):MHWRender::MPxShaderOverride(obj), fShaderNode(NULL){}

	CurvatureShader *fShaderNode;
};