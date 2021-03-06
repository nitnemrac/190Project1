/************************************************************************************

Authors     :   Bradley Austin Davis <bdavis@saintandreas.org>
Copyright   :   Copyright Brad Davis. All Rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/


#include <iostream>
#include <memory>
#include <exception>
#include <algorithm>
#include <ctime>

#include <Windows.h>
#include "Shader.h"
#include "Model.h"
#include"Line.h"

#define __STDC_FORMAT_MACROS 1
#define LEFT 0
#define RIGHT 1

#define FAIL(X) throw std::runtime_error(X)

///////////////////////////////////////////////////////////////////////////////
//
// GLM is a C++ math library meant to mirror the syntax of GLSL 
//

#include <glm/glm.hpp>
#include <glm/gtc/noise.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <math.h>
#include "Extras/OVR_Math.h"

// Import the most commonly used types into the default namespace
using glm::ivec3;
using glm::ivec2;
using glm::uvec2;
using glm::mat3;
using glm::mat4;
using glm::vec2;
using glm::vec3;
using glm::vec4;
using glm::quat;

///////////////////////////////////////////////////////////////////////////////
//
// GLEW gives cross platform access to OpenGL 3.x+ functionality.  
//

#include <GL/glew.h>

bool checkFramebufferStatus(GLenum target = GL_FRAMEBUFFER) {
	GLuint status = glCheckFramebufferStatus(target);
	switch (status) {
	case GL_FRAMEBUFFER_COMPLETE:
		return true;
		break;

	case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
		std::cerr << "framebuffer incomplete attachment" << std::endl;
		break;

	case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
		std::cerr << "framebuffer missing attachment" << std::endl;
		break;

	case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
		std::cerr << "framebuffer incomplete draw buffer" << std::endl;
		break;

	case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
		std::cerr << "framebuffer incomplete read buffer" << std::endl;
		break;

	case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
		std::cerr << "framebuffer incomplete multisample" << std::endl;
		break;

	case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
		std::cerr << "framebuffer incomplete layer targets" << std::endl;
		break;

	case GL_FRAMEBUFFER_UNSUPPORTED:
		std::cerr << "framebuffer unsupported internal format or image" << std::endl;
		break;

	default:
		std::cerr << "other framebuffer error" << std::endl;
		break;
	}

	return false;
}

bool checkGlError() {
	GLenum error = glGetError();
	if (!error) {
		return false;
	}
	else {
		switch (error) {
		case GL_INVALID_ENUM:
			std::cerr << ": An unacceptable value is specified for an enumerated argument.The offending command is ignored and has no other side effect than to set the error flag.";
			break;
		case GL_INVALID_VALUE:
			std::cerr << ": A numeric argument is out of range.The offending command is ignored and has no other side effect than to set the error flag";
			break;
		case GL_INVALID_OPERATION:
			std::cerr << ": The specified operation is not allowed in the current state.The offending command is ignored and has no other side effect than to set the error flag..";
			break;
		case GL_INVALID_FRAMEBUFFER_OPERATION:
			std::cerr << ": The framebuffer object is not complete.The offending command is ignored and has no other side effect than to set the error flag.";
			break;
		case GL_OUT_OF_MEMORY:
			std::cerr << ": There is not enough memory left to execute the command.The state of the GL is undefined, except for the state of the error flags, after this error is recorded.";
			break;
		case GL_STACK_UNDERFLOW:
			std::cerr << ": An attempt has been made to perform an operation that would cause an internal stack to underflow.";
			break;
		case GL_STACK_OVERFLOW:
			std::cerr << ": An attempt has been made to perform an operation that would cause an internal stack to overflow.";
			break;
		}
		return true;
	}
}

void glDebugCallbackHandler(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *msg, GLvoid* data) {
	OutputDebugStringA(msg);
	std::cout << "debug call: " << msg << std::endl;
}

//////////////////////////////////////////////////////////////////////
//
// GLFW provides cross platform window creation
//

#include <GLFW/glfw3.h>

namespace glfw {
	inline GLFWwindow * createWindow(const uvec2 & size, const ivec2 & position = ivec2(INT_MIN)) {
		GLFWwindow * window = glfwCreateWindow(size.x, size.y, "glfw", nullptr, nullptr);
		if (!window) {
			FAIL("Unable to create rendering window");
		}
		if ((position.x > INT_MIN) && (position.y > INT_MIN)) {
			glfwSetWindowPos(window, position.x, position.y);
		}
		return window;
	}
}

// A class to encapsulate using GLFW to handle input and render a scene
class GlfwApp {

protected:
	uvec2 windowSize;
	ivec2 windowPosition;
	GLFWwindow * window{ nullptr };
	unsigned int frame{ 0 };

public:
	GlfwApp() {
		// Initialize the GLFW system for creating and positioning windows
		if (!glfwInit()) {
			FAIL("Failed to initialize GLFW");
		}
		glfwSetErrorCallback(ErrorCallback);
	}

	virtual ~GlfwApp() {
		if (nullptr != window) {
			glfwDestroyWindow(window);
		}
		glfwTerminate();
	}

	virtual int run() {
		preCreate();

		window = createRenderingTarget(windowSize, windowPosition);

		if (!window) {
			std::cout << "Unable to create OpenGL window" << std::endl;
			return -1;
		}

		postCreate();

		initGl();

		while (!glfwWindowShouldClose(window)) {
			++frame;
			glfwPollEvents();
			update();
			draw();
			finishFrame();
		}

		shutdownGl();

		return 0;
	}


protected:
	virtual GLFWwindow * createRenderingTarget(uvec2 & size, ivec2 & pos) = 0;

	virtual void draw() = 0;

	void preCreate() {
		glfwWindowHint(GLFW_DEPTH_BITS, 16);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
	}


	void postCreate() {
		glfwSetWindowUserPointer(window, this);
		glfwSetKeyCallback(window, KeyCallback);
		glfwSetMouseButtonCallback(window, MouseButtonCallback);
		glfwMakeContextCurrent(window);

		// Initialize the OpenGL bindings
		// For some reason we have to set this experminetal flag to properly
		// init GLEW if we use a core context.
		glewExperimental = GL_TRUE;
		if (0 != glewInit()) {
			FAIL("Failed to initialize GLEW");
		}
		glGetError();

		if (GLEW_KHR_debug) {
			GLint v;
			glGetIntegerv(GL_CONTEXT_FLAGS, &v);
			if (v & GL_CONTEXT_FLAG_DEBUG_BIT) {
				//glDebugMessageCallback(glDebugCallbackHandler, this);
			}
		}
	}

	virtual void initGl() {
	}

	virtual void shutdownGl() {
	}

	virtual void finishFrame() {
		glfwSwapBuffers(window);
	}

	virtual void destroyWindow() {
		glfwSetKeyCallback(window, nullptr);
		glfwSetMouseButtonCallback(window, nullptr);
		glfwDestroyWindow(window);
	}

	virtual void onKey(int key, int scancode, int action, int mods) {
		if (GLFW_PRESS != action) {
			return;
		}

		switch (key) {
		case GLFW_KEY_ESCAPE:
			glfwSetWindowShouldClose(window, 1);
			return;
		}
	}

	virtual void update() {}

	virtual void onMouseButton(int button, int action, int mods) {}

protected:
	virtual void viewport(const ivec2 & pos, const uvec2 & size) {
		glViewport(pos.x, pos.y, size.x, size.y);
	}

private:

	static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
		GlfwApp * instance = (GlfwApp *)glfwGetWindowUserPointer(window);
		instance->onKey(key, scancode, action, mods);
	}

	static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
		GlfwApp * instance = (GlfwApp *)glfwGetWindowUserPointer(window);
		instance->onMouseButton(button, action, mods);
	}

	static void ErrorCallback(int error, const char* description) {
		FAIL(description);
	}
};

//////////////////////////////////////////////////////////////////////
//
// The Oculus VR C API provides access to information about the HMD
//

#include <OVR_CAPI.h>
#include <OVR_CAPI_GL.h>

namespace ovr {

	// Convenience method for looping over each eye with a lambda
	template <typename Function>
	inline void for_each_eye(Function function) {
		for (ovrEyeType eye = ovrEyeType::ovrEye_Left;
			eye < ovrEyeType::ovrEye_Count;
			eye = static_cast<ovrEyeType>(eye + 1)) {
			function(eye);
		}
	}

	inline mat4 toGlm(const ovrMatrix4f & om) {
		return glm::transpose(glm::make_mat4(&om.M[0][0]));
	}

	inline mat4 toGlm(const ovrFovPort & fovport, float nearPlane = 0.01f, float farPlane = 10000.0f) {
		return toGlm(ovrMatrix4f_Projection(fovport, nearPlane, farPlane, true));
	}

	inline vec3 toGlm(const ovrVector3f & ov) {
		return glm::make_vec3(&ov.x);
	}

	inline vec2 toGlm(const ovrVector2f & ov) {
		return glm::make_vec2(&ov.x);
	}

	inline uvec2 toGlm(const ovrSizei & ov) {
		return uvec2(ov.w, ov.h);
	}

	inline quat toGlm(const ovrQuatf & oq) {
		return glm::make_quat(&oq.x);
	}

	inline mat4 toGlm(const ovrPosef & op) {
		mat4 orientation = glm::mat4_cast(toGlm(op.Orientation));
		mat4 translation = glm::translate(mat4(), ovr::toGlm(op.Position));
		return translation * orientation;
	}

	inline ovrMatrix4f fromGlm(const mat4 & m) {
		ovrMatrix4f result;
		mat4 transposed(glm::transpose(m));
		memcpy(result.M, &(transposed[0][0]), sizeof(float) * 16);
		return result;
	}

	inline ovrVector3f fromGlm(const vec3 & v) {
		ovrVector3f result;
		result.x = v.x;
		result.y = v.y;
		result.z = v.z;
		return result;
	}

	inline ovrVector2f fromGlm(const vec2 & v) {
		ovrVector2f result;
		result.x = v.x;
		result.y = v.y;
		return result;
	}

	inline ovrSizei fromGlm(const uvec2 & v) {
		ovrSizei result;
		result.w = v.x;
		result.h = v.y;
		return result;
	}

	inline ovrQuatf fromGlm(const quat & q) {
		ovrQuatf result;
		result.x = q.x;
		result.y = q.y;
		result.z = q.z;
		result.w = q.w;
		return result;
	}
}

class RiftManagerApp {
protected:
	ovrSession _session;
	ovrHmdDesc _hmdDesc;
	ovrGraphicsLuid _luid;

public:
	RiftManagerApp() {
		if (!OVR_SUCCESS(ovr_Create(&_session, &_luid))) {
			FAIL("Unable to create HMD session");
		}

		_hmdDesc = ovr_GetHmdDesc(_session);
	}

	~RiftManagerApp() {
		ovr_Destroy(_session);
		_session = nullptr;
	}
};

class RiftApp : public GlfwApp, public RiftManagerApp {
public:

private:
	GLuint _fbo{ 0 };
	GLuint _depthBuffer{ 0 };
	ovrTextureSwapChain _eyeTexture;

	GLuint _mirrorFbo{ 0 };
	ovrMirrorTexture _mirrorTexture;

	ovrEyeRenderDesc _eyeRenderDescs[2];

	mat4 _eyeProjections[2];

	ovrLayerEyeFov _sceneLayer;
	ovrViewScaleDesc _viewScaleDesc;

	uvec2 _renderTargetSize;
	uvec2 _mirrorSize;

public:

	RiftApp() {
		using namespace ovr;
		_viewScaleDesc.HmdSpaceToWorldScaleInMeters = 1.0f;

		memset(&_sceneLayer, 0, sizeof(ovrLayerEyeFov));
		_sceneLayer.Header.Type = ovrLayerType_EyeFov;
		_sceneLayer.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft;

		ovr::for_each_eye([&](ovrEyeType eye) {
			ovrEyeRenderDesc& erd = _eyeRenderDescs[eye] = ovr_GetRenderDesc(_session, eye, _hmdDesc.DefaultEyeFov[eye]);
			ovrMatrix4f ovrPerspectiveProjection =
				ovrMatrix4f_Projection(erd.Fov, 0.01f, 1000.0f, ovrProjection_ClipRangeOpenGL);
			_eyeProjections[eye] = ovr::toGlm(ovrPerspectiveProjection);
			_viewScaleDesc.HmdToEyeOffset[eye] = erd.HmdToEyeOffset;

			ovrFovPort & fov = _sceneLayer.Fov[eye] = _eyeRenderDescs[eye].Fov;
			auto eyeSize = ovr_GetFovTextureSize(_session, eye, fov, 1.0f);
			_sceneLayer.Viewport[eye].Size = eyeSize;
			_sceneLayer.Viewport[eye].Pos = { (int)_renderTargetSize.x, 0 };

			_renderTargetSize.y = std::max(_renderTargetSize.y, (uint32_t)eyeSize.h);
			_renderTargetSize.x += eyeSize.w;
		});
		// Make the on screen window 1/4 the resolution of the render target
		_mirrorSize = _renderTargetSize;
		_mirrorSize /= 4;
	}

protected:
	GLFWwindow * createRenderingTarget(uvec2 & outSize, ivec2 & outPosition) override {
		return glfw::createWindow(_mirrorSize);
	}

	void initGl() override {
		GlfwApp::initGl();

		// Disable the v-sync for buffer swap
		glfwSwapInterval(0);

		ovrTextureSwapChainDesc desc = {};
		desc.Type = ovrTexture_2D;
		desc.ArraySize = 1;
		desc.Width = _renderTargetSize.x;
		desc.Height = _renderTargetSize.y;
		desc.MipLevels = 1;
		desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
		desc.SampleCount = 1;
		desc.StaticImage = ovrFalse;
		ovrResult result = ovr_CreateTextureSwapChainGL(_session, &desc, &_eyeTexture);
		_sceneLayer.ColorTexture[0] = _eyeTexture;
		if (!OVR_SUCCESS(result)) {
			FAIL("Failed to create swap textures");
		}

		int length = 0;
		result = ovr_GetTextureSwapChainLength(_session, _eyeTexture, &length);
		if (!OVR_SUCCESS(result) || !length) {
			FAIL("Unable to count swap chain textures");
		}
		for (int i = 0; i < length; ++i) {
			GLuint chainTexId;
			ovr_GetTextureSwapChainBufferGL(_session, _eyeTexture, i, &chainTexId);
			glBindTexture(GL_TEXTURE_2D, chainTexId);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}
		glBindTexture(GL_TEXTURE_2D, 0);

		// Set up the framebuffer object
		glGenFramebuffers(1, &_fbo);
		glGenRenderbuffers(1, &_depthBuffer);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _fbo);
		glBindRenderbuffer(GL_RENDERBUFFER, _depthBuffer);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, _renderTargetSize.x, _renderTargetSize.y);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
		glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _depthBuffer);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

		ovrMirrorTextureDesc mirrorDesc;
		memset(&mirrorDesc, 0, sizeof(mirrorDesc));
		mirrorDesc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
		mirrorDesc.Width = _mirrorSize.x;
		mirrorDesc.Height = _mirrorSize.y;
		if (!OVR_SUCCESS(ovr_CreateMirrorTextureGL(_session, &mirrorDesc, &_mirrorTexture))) {
			FAIL("Could not create mirror texture");
		}
		glGenFramebuffers(1, &_mirrorFbo);
	}

	void onKey(int key, int scancode, int action, int mods) override {
		if (GLFW_PRESS == action) switch (key) {
		case GLFW_KEY_R:
			ovr_RecenterTrackingOrigin(_session);
			return;
		}

		GlfwApp::onKey(key, scancode, action, mods);
	}

	void draw() final override {
		ovrPosef eyePoses[2];
		ovr_GetEyePoses(_session, frame, true, _viewScaleDesc.HmdToEyeOffset, eyePoses, &_sceneLayer.SensorSampleTime);

		int curIndex;
		ovr_GetTextureSwapChainCurrentIndex(_session, _eyeTexture, &curIndex);
		GLuint curTexId;
		ovr_GetTextureSwapChainBufferGL(_session, _eyeTexture, curIndex, &curTexId);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _fbo);
		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, curTexId, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		vec3 eyepos = { (eyePoses[0].Position.x + eyePoses[1].Position.x) / 2,
			(eyePoses[0].Position.y + eyePoses[1].Position.y) / 2,
			(eyePoses[0].Position.z + eyePoses[1].Position.z) / 2 };

		ovr::for_each_eye([&](ovrEyeType eye) {
			const auto& vp = _sceneLayer.Viewport[eye];
			glViewport(vp.Pos.x, vp.Pos.y, vp.Size.w, vp.Size.h);
			_sceneLayer.RenderPose[eye] = eyePoses[eye];
			renderScene(_eyeProjections[eye], ovr::toGlm(eyePoses[eye]), eyepos);
		});
		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		ovr_CommitTextureSwapChain(_session, _eyeTexture);
		ovrLayerHeader* headerList = &_sceneLayer.Header;
		ovr_SubmitFrame(_session, frame, &_viewScaleDesc, &headerList, 1);

		GLuint mirrorTextureId;
		ovr_GetMirrorTextureBufferGL(_session, _mirrorTexture, &mirrorTextureId);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, _mirrorFbo);
		glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mirrorTextureId, 0);
		glBlitFramebuffer(0, 0, _mirrorSize.x, _mirrorSize.y, 0, _mirrorSize.y, _mirrorSize.x, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	}

	virtual void renderScene(const glm::mat4 & projection, const glm::mat4 & headPose, glm::vec3 eyepos) = 0;
};

//////////////////////////////////////////////////////////////////////
//
// The remainder of this code is specific to the scene we want to 
// render.  I use oglplus to render an array of cubes, but your 
// application would perform whatever rendering you want
//


//////////////////////////////////////////////////////////////////////
//
// OGLplus is a set of wrapper classes for giving OpenGL a more object
// oriented interface
//
#define OGLPLUS_USE_GLCOREARB_H 0
#define OGLPLUS_USE_GLEW 1
#define OGLPLUS_USE_BOOST_CONFIG 0
#define OGLPLUS_NO_SITE_CONFIG 1
#define OGLPLUS_LOW_PROFILE 1

#pragma warning( disable : 4068 4244 4267 4065)
#include <oglplus/config/basic.hpp>
#include <oglplus/config/gl.hpp>
#include <oglplus/all.hpp>
#include <oglplus/interop/glm.hpp>
#include <oglplus/bound/texture.hpp>
#include <oglplus/bound/framebuffer.hpp>
#include <oglplus/bound/renderbuffer.hpp>
#include <oglplus/bound/buffer.hpp>
#include <oglplus/shapes/cube.hpp>
#include <oglplus/shapes/wrapper.hpp>
#pragma warning( default : 4068 4244 4267 4065)



namespace Attribute {
	enum {
		Position = 0,
		TexCoord0 = 1,
		Normal = 2,
		Color = 3,
		TexCoord1 = 4,
		InstanceTransform = 5,
	};
}

struct Particle {
	Model* model;
	glm::mat4 transform;
	glm::vec3 velocity;
	glm::vec3 rotation;
};

// a class for encapsulating building and rendering an RGB cube
struct ColorCubeScene {

	// Program
	oglplus::shapes::ShapeWrapper cube;
	oglplus::Program prog;
	oglplus::VertexArray vao;
	GLuint instanceCount;
	oglplus::Buffer instances;

	Model* factory;
	Model* co2;
	Model* o2;
	Model* greenLaser;
	Model* redLaser;
	int co2Count = 0;


	GLuint shaderProg;
	std::vector<Particle*> particles;
	Particle factoryParticle;
	Particle leftLaser;
	Particle rightLaser;

	std::clock_t timer;
	std::clock_t vibTimer;
	bool win = false;
	bool lose = false;

	glm::mat4 chimney = glm::mat4(1.0f, 0, 0, 0, 0, 1.0f, 0, 0, 0, 0, 1.0f, 0, 0, -1, -15, 1.0f);

	// For controller input
	ovrTrackingState trackstate;
	ovrPosef handPoses[2];
	ovrInputState inputstate;
	bool fingerTriggerPressed[2] = { false, false };

	ovrSession sesh; //Needed for haptic feedback

	// VBOs for the cube's vertices and normals

	const unsigned int GRID_SIZE{ 5 };

public:
	ColorCubeScene() : cube({ "Position", "Normal" }, oglplus::shapes::Cube()) {
		shaderProg = LoadShaders("shader.vert", "shader.frag");
		factory = new Model("../Project1-assets/factory4/factory4.obj");
		//factory2 = new Model("../Project1-assets/factory2/factory2.obj");
		co2 = new Model("../Project1-assets/co2/co2.obj");
		o2 = new Model("../Project1-assets/o2/o2.obj");
		greenLaser = new Model("../Project1-assets/cylinder/cylinder_green.obj");
		redLaser = new Model("../Project1-assets/cylinder/cylinder_red.obj");


		//rightLine = new Line();
		factoryParticle.model = factory;
		factoryParticle.transform = glm::scale(chimney, glm::vec3(0.2f, 0.2f, 0.2f));
		leftLaser.model = greenLaser;
		rightLaser.model = greenLaser;
		for (int i = 0; i < 5; i++) {
			Particle* p = new Particle();
			p->model = co2;
			p->transform = glm::scale(chimney, glm::vec3(0.3f));
			p->velocity = glm::normalize(glm::vec3(fmod(rand(), 100.f) - 50, fmod(rand(), 100.0f), fmod(rand(), 100.0f) - 50)) / 100.0f;
			p->rotation = glm::normalize(glm::vec3(fmod(rand(), 100.f) - 50, fmod(rand(), 100.f) - 50, fmod(rand(), 100.f) - 50));
			particles.push_back(p);
		}
		co2Count = 5;
		timer = std::clock();
	}

	void render(const mat4 & projection, const mat4 & modelview, ovrSession session, glm::vec3 eyepos) {
		getControllerData(session);
		glUseProgram(shaderProg);

		GLuint uProjection = glGetUniformLocation(shaderProg, "projection");
		GLuint uModelview = glGetUniformLocation(shaderProg, "view");
		GLuint uTransMat = glGetUniformLocation(shaderProg, "model");
		GLuint uEyePos = glGetUniformLocation(shaderProg, "eyepos");

		glUniformMatrix4fv(uProjection, 1, GL_FALSE, (&projection[0][0]));
		glUniformMatrix4fv(uModelview, 1, GL_FALSE, &(modelview[0][0]));
		glUniform3f(uEyePos, eyepos.x, eyepos.y, eyepos.z);

		//LEFT HAND-----------------------------------------------------------
		/*float yawy, pitchx, rollz;
		OVR::Quatf leftori = handPoses[LEFT].Orientation;
		leftori.GetEulerAngles<OVR::Axis_Y, OVR::Axis_X, OVR::Axis_Z>(&yawy, &pitchx, &rollz);*/

		glm::quat q = ovr::toGlm(handPoses[LEFT].Orientation);
		glm::mat4 rotmat = glm::toMat4(q);

		glm::mat4 lasertransform;
		lasertransform = glm::translate(lasertransform, glm::vec3(handPoses[LEFT].Position.x, handPoses[LEFT].Position.y, handPoses[LEFT].Position.z));
		lasertransform = lasertransform * rotmat;
		lasertransform = glm::scale(lasertransform, glm::vec3(0.01f, 0.01f, -20.f));
		glUniformMatrix4fv(uTransMat, 1, GL_FALSE, &lasertransform[0][0]);
		leftLaser.transform = lasertransform;
		leftLaser.model->Draw(shaderProg);


		//RIGHT HAND----------------------------------------------------------
		q = ovr::toGlm(handPoses[RIGHT].Orientation);
		rotmat = glm::toMat4(q);

		lasertransform = glm::mat4(1.0f);
		lasertransform = glm::translate(lasertransform, glm::vec3(handPoses[RIGHT].Position.x, handPoses[RIGHT].Position.y, handPoses[RIGHT].Position.z));
		lasertransform = lasertransform * rotmat;
		lasertransform = glm::scale(lasertransform, glm::vec3(0.01f, 0.01f, -20.f));
		glUniformMatrix4fv(uTransMat, 1, GL_FALSE, &lasertransform[0][0]);
		rightLaser.transform = lasertransform;
		rightLaser.model->Draw(shaderProg);

		glUniformMatrix4fv(uTransMat, 1, GL_FALSE, &factoryParticle.transform[0][0]);
		factoryParticle.model->Draw(shaderProg);

		for (Particle* particle : particles) {
			glUniformMatrix4fv(uTransMat, 1, GL_FALSE, &(particle->transform[0][0]));
			particle->model->Draw(shaderProg);
		}
				

		update();
	} 

	void getControllerData(ovrSession session) {
		
		// Position + Orientation
		double displayMidpointSeconds = ovr_GetPredictedDisplayTime(session, 0);
		trackstate = ovr_GetTrackingState(session, displayMidpointSeconds, ovrTrue);

		handPoses[LEFT] = trackstate.HandPoses[ovrHand_Left].ThePose;
		handPoses[RIGHT] = trackstate.HandPoses[ovrHand_Right].ThePose;


		// T R I G G E R E D
		// finger triggers
		if (OVR_SUCCESS(ovr_GetInputState(session, ovrControllerType_Touch, &inputstate))) {
			fingerTriggerPressed[LEFT] = inputstate.IndexTrigger[ovrHand_Left] > 0.5f;
			fingerTriggerPressed[RIGHT] = inputstate.IndexTrigger[ovrHand_Right] > 0.5f;
		}

		sesh = session;

		/*if (fingerTriggerPressed[LEFT]) {
			printf("LEFT trigger\n");
		}
		if (fingerTriggerPressed[RIGHT]) {
			printf("RIGHT trigger\n");
		}
		if (inputstate.Buttons != 0) {
			printf("BUTTONS PRESSED\n");
		}*/
	}

	void update() {

		//Haptic feedback check
		if (!fingerTriggerPressed[LEFT] || !fingerTriggerPressed[RIGHT] || (1000.0f * (std::clock() - vibTimer)) / CLOCKS_PER_SEC > 100.0f) {
			ovr_SetControllerVibration(sesh, ovrControllerType_LTouch, 0.0f, 0.0f);
			ovr_SetControllerVibration(sesh, ovrControllerType_RTouch, 0.0f, 0.0f);
		}

		for (Particle* particle : particles) {
			//Update position
			particle->transform[3] += glm::vec4(particle->velocity.x, particle->velocity.y, particle->velocity.z , 0.0f);
			particle->transform = glm::rotate(particle->transform, 0.05f, glm::vec3(particle->rotation.x, particle->rotation.y, particle->rotation.z));
			//cout << particle->transform[3].z << endl;
			//Check walls
			if (particle->transform[3][0] < -10 || particle->transform[3][0] > 10) {
				particle->velocity.x *= -1;
			}
			if (particle->transform[3][1] < -10 || particle->transform[3][1] > 10) {
				particle->velocity.y *= -1;
			}
			if (particle->transform[3][2] < -25 || particle->transform[3][2] > -5) {
				particle->velocity.z *= -1;
			}

			//If CO2 and intersected, change model to O2
			if (particle->model == co2 && leftLaser.model == redLaser && rightLaser.model == redLaser) {
				bool left = false;
				bool right = false;

				//Check left intersection
				//Get two points on the line
				glm::vec3 start = glm::vec3(leftLaser.transform[3].x, leftLaser.transform[3].y, leftLaser.transform[3].z);
				glm::vec4 endToTrans = glm::vec4(0, 0, -1, 1);
				endToTrans = leftLaser.transform * endToTrans;
				glm::vec3 endPt = glm::vec3(endToTrans.x, endToTrans.y, endToTrans.z);
				glm::vec3 particleCenter = glm::vec3(particle->transform[3]);
				//Point-line dist
				float dist = glm::length(glm::cross(endPt - start, start - particleCenter)) / glm::length(endPt - start);
				if (dist <= 0.3f) left = true;

				//Check right intersection
				//Get two points on the line
				start = glm::vec3(rightLaser.transform[3].x, rightLaser.transform[3].y, rightLaser.transform[3].z);
				endToTrans = glm::vec4(0, 0, -1, 1);
				endToTrans = rightLaser.transform * endToTrans;
				endPt = glm::vec3(endToTrans.x, endToTrans.y, endToTrans.z);
				//Point-line dist
				dist = glm::length(glm::cross(endPt - start, start - particleCenter)) / glm::length(endPt - start);
				if (dist <= 0.3f) right = true;

				if (left && right) {
					particle->model = o2;
					ovr_SetControllerVibration(sesh, ovrControllerType_LTouch, 0.0f, 1.0f);
					ovr_SetControllerVibration(sesh, ovrControllerType_RTouch, 0.0f, 1.0f);
					vibTimer = std::clock();
					co2Count--;
				}
			}
		}

		//If index trigger pressed, red laser
		if (inputstate.IndexTrigger[ovrHand_Left] > 0.5f) {
			leftLaser.model = redLaser;
		}
		//Else green laser
		else {
			leftLaser.model = greenLaser;
		}
		if (inputstate.IndexTrigger[ovrHand_Right] > 0.5f) {
			rightLaser.model = redLaser;
		}
		else {
			rightLaser.model = greenLaser;
		}
		
		//Add particles if haven't won and a second has passed
		if (!win) {
			std::clock_t currentTime = std::clock();
			if ((currentTime - timer) / CLOCKS_PER_SEC >= 1) {
				Particle* p = new Particle();
				p->model = co2;
				p->transform = glm::scale(chimney, glm::vec3(0.3f));
				p->velocity = glm::normalize(glm::vec3(fmod(rand(), 100.f) - 50, fmod(rand(), 100.0f), fmod(rand(), 100.0f) - 50)) / 100.0f;
				p->rotation = glm::normalize(glm::vec3(fmod(rand(), 100.f) - 50, fmod(rand(), 100.f) - 50, fmod(rand(), 100.f) - 50));
				particles.push_back(p);

				co2Count++;
				timer = currentTime;
			}
		}

		//Loss case
		if (co2Count > 10 && !lose) {
			for (int i = 0; i < 100; i++) {
				Particle* p = new Particle();
				p->model = co2;
				p->transform = glm::translate(glm::vec3(fmod(rand(), 20) - 10, fmod(rand(), 20) - 10, fmod(rand(), 20) - 25));
				p->transform = glm::scale(p->transform, glm::vec3(0.3f));
				p->velocity = glm::normalize(glm::vec3(fmod(rand(), 100.f) - 50, fmod(rand(), 100.0f), fmod(rand(), 100.0f) - 50)) / 100.0f;
				p->rotation = glm::normalize(glm::vec3(fmod(rand(), 100.f) - 50, fmod(rand(), 100.f) - 50, fmod(rand(), 100.f) - 50));
				particles.push_back(p);
			}

			lose = true;
		}

		//Win case
		if (co2Count == 0 && !lose) {
			glClearColor(0.0f, 0.2f, 0.8f, 0.0f);
			win = true;
		}

		//Game reset
		if ((win || lose) && inputstate.Buttons != 0) {
			win = false;
			lose = false;
			particles = std::vector<Particle*>();
			for (int i = 0; i < 5; i++) {
			Particle* p = new Particle();
			p->model = co2;
			p->transform = glm::scale(chimney, glm::vec3(0.3f));
			p->velocity = glm::normalize(glm::vec3(fmod(rand(), 100.f) - 50, fmod(rand(), 100.0f), fmod(rand(), 100.0f) - 50)) / 100.0f;
			p->rotation = glm::normalize(glm::vec3(fmod(rand(), 100.f) - 50, fmod(rand(), 100.f) - 50, fmod(rand(), 100.f) - 50));
			particles.push_back(p);
			}
			co2Count = 5;

			glClearColor(0.0f, 0.0f, 0.4f, 0.0f);
		}
	}
};


// An example application that renders a simple cube
class ExampleApp : public RiftApp {
	std::shared_ptr<ColorCubeScene> cubeScene;

public:
	ExampleApp() { }

protected:
	void initGl() override {
		RiftApp::initGl();
		glClearColor(0.0f, 0.0f, 0.4f, 0.0f);
		glEnable(GL_DEPTH_TEST);
		ovr_RecenterTrackingOrigin(_session);
		cubeScene = std::shared_ptr<ColorCubeScene>(new ColorCubeScene());
	}

	void shutdownGl() override {
		cubeScene.reset();
	}

	void renderScene(const glm::mat4 & projection, const glm::mat4 & headPose, glm::vec3 eyepos) override {
		cubeScene->render(projection, glm::inverse(headPose), _session, eyepos);
	}
};

// Execute our example class
int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	AllocConsole();
	freopen("conin$", "r", stdin);
	freopen("conout$", "w", stdout);
	freopen("conout$", "w", stderr);
	int result = -1;
	try {
		if (!OVR_SUCCESS(ovr_Initialize(nullptr))) {
			FAIL("Failed to initialize the Oculus SDK");
		}
		result = ExampleApp().run();
	}
	catch (std::exception & error) {
		OutputDebugStringA(error.what());
		std::cerr << error.what() << std::endl;
	}
	ovr_Shutdown();
	return result;
}