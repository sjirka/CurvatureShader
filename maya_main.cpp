#include <maya/MFnPlugin.h>

#include "curvatureShader.h"
#include "CurvatureShaderOverride.h"

MStatus initializePlugin(MObject obj)
{
	MStatus status(MStatus::kSuccess);
	MFnPlugin plugin(obj, "Stepan Jirka", "1.0");

	const MString classification = ("shader/surface/utility/:drawdb/shader/surface/curvatureShader");
	status = plugin.registerNode(	"curvatureShader",
									CurvatureShader::typeId,
									CurvatureShader::creator,
									CurvatureShader::initialize,
									MPxNode::kHwShaderNode,
									&classification);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	
	status = MHWRender::MDrawRegistry::registerShaderOverrideCreator(
		"drawdb/shader/surface/curvatureShader",
		CurvatureShaderOverride::registrantId,
		CurvatureShaderOverride::Creator);
	CHECK_MSTATUS_AND_RETURN_IT(status);

	CurvatureShader::callbacks.append(MDGMessage::addConnectionCallback(CurvatureShader::preConnection, NULL, &status));

	return status;
}

MStatus uninitializePlugin(MObject obj)
{
	MStatus status(MStatus::kSuccess);
	MFnPlugin plugin(obj);

	MMessage::removeCallbacks(CurvatureShader::callbacks);

	status = MHWRender::MDrawRegistry::deregisterShaderOverrideCreator(
		"drawdb/shader/surface/curvatureShader", CurvatureShaderOverride::registrantId);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	
	status = plugin.deregisterNode(CurvatureShader::typeId);
	CHECK_MSTATUS_AND_RETURN_IT(status);

	return status;
}

