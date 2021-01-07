#include <jni.h>
#include <string>
#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraDevice.h>
#include <android/native_window_jni.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include "SLog.h"
#include "linmath.h"

//https://www.sisik.eu/blog/android/ndk/camera

static ACameraManager *sCameraManager = nullptr;
static ACameraDevice *cameraDevice = nullptr;
static ANativeWindow* textureWindow = nullptr;
static ACameraOutputTarget* textureTarget = nullptr;
static ACaptureSessionOutput* textureOutput = nullptr;
static ACaptureSessionOutputContainer* outputs = nullptr;
//static ACaptureSessionOutput* output = nullptr;
static ACaptureRequest* request = nullptr;
static ACameraCaptureSession* textureSession = nullptr;

/**
 * GL stuff - mostly used to draw the frames captured
 * by camera into a SurfaceTexture
 */

static GLuint prog;
static GLuint vtxShader;
static GLuint fragShader;

static GLint vtxPosAttrib;
static GLint uvsAttrib;
static GLint mvpMatrix;
static GLint texMatrix;
static GLint texSampler;
static GLint color;
static GLint size;
static GLuint buf[2];
static GLuint textureId;


static int width = 640;
static int height = 480;

static void onDisconnected(void *context, ACameraDevice *device) {
    LOGD("onDisconnected");
}

static void onError(void *context, ACameraDevice *device, int error) {
    LOGD("error %d", error);
}

static ACameraDevice_stateCallbacks cameraDeviceCallbacks = {
        .context = nullptr,
        .onDisconnected = onDisconnected,
        .onError = onError,
};

/**
 * Session state callbacks
 */

static void onSessionActive(void *context, ACameraCaptureSession *session) {
    LOGD("onSessionActive()");
}

static void onSessionReady(void *context, ACameraCaptureSession *session) {
    LOGD("onSessionReady()");
}

static void onSessionClosed(void *context, ACameraCaptureSession *session) {
    LOGD("onSessionClosed()");
}

static ACameraCaptureSession_stateCallbacks sessionStateCallbacks{
        .context = nullptr,
        .onActive = onSessionActive,
        .onReady = onSessionReady,
        .onClosed = onSessionClosed
};

/**
 * Capture callbacks
 */

void onCaptureFailed(void* context, ACameraCaptureSession* session,
                     ACaptureRequest* request, ACameraCaptureFailure* failure)
{
    LOGE("onCaptureFailed ");
}

void onCaptureSequenceCompleted(void* context, ACameraCaptureSession* session,
                                int sequenceId, int64_t frameNumber)
{}

void onCaptureSequenceAborted(void* context, ACameraCaptureSession* session,
                              int sequenceId)
{}

void onCaptureCompleted (
        void* context, ACameraCaptureSession* session,
        ACaptureRequest* request, const ACameraMetadata* result)
{
    LOGD("Capture completed");
}

static ACameraCaptureSession_captureCallbacks captureCallbacks {
        .context = nullptr,
        .onCaptureStarted = nullptr,
        .onCaptureProgressed = nullptr,
        .onCaptureCompleted = onCaptureCompleted,
        .onCaptureFailed = onCaptureFailed,
        .onCaptureSequenceCompleted = onCaptureSequenceCompleted,
        .onCaptureSequenceAborted = onCaptureSequenceAborted,
        .onCaptureBufferLost = nullptr,
};

///////////////////////////////// Shaders ///////////////////////////////

static const char* vertexShaderSrc = R"(
        precision highp float;
        attribute vec3 vertexPosition;
        attribute vec2 uvs;
        varying vec2 varUvs;
        uniform mat4 texMatrix;
        uniform mat4 mvp;
        void main()
        {
            varUvs = (texMatrix * vec4(uvs.x,uvs.y, 0, 1.0)).xy;
            gl_Position = mvp * vec4(vertexPosition, 1.0);
        }
)";

static const char* fragmentShaderSrc = R"(
        #extension GL_OES_EGL_image_external : require
        precision mediump float;
        varying vec2 varUvs;
        uniform samplerExternalOES texSampler;
         uniform vec2 size;
        void main()
        {
           // vec2 uv = vec2(gl_FragCoord.x / size.x,gl_FragCoord.y / size.y);
            gl_FragColor = texture2D(texSampler, varUvs);// * color;
            /*
            if (gl_FragCoord.x/size.x < 0.5) {
                gl_FragColor = texture2D(texSampler, varUvs) * color;
            }
            else {
                const float threshold = 1.1;
                vec4 c = texture2D(texSampler, varUvs);
                if (length(c) > threshold) {
                    gl_FragColor = vec4(0.0, 0.0, 1.0, 1.0);
                } else {
                    gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
                }
            }
            */
        }
)";
/////////////////////////////////////////////////////////////////////////

GLuint createShader(const char *src, GLenum type)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);

    GLint isCompiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
    if (isCompiled == GL_FALSE)
    {
        GLint maxLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
        GLchar *logStr = (GLchar*)malloc(maxLength);

        glGetShaderInfoLog(shader, maxLength, &maxLength, logStr);
        LOGE("Could not compile shader %s - %s", src, logStr);
        free(logStr);
    }

    return shader;
}

GLuint createProgram(GLuint vertexShader, GLuint fragmentShader)
{
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vertexShader);
    glAttachShader(prog, fragmentShader);
    glLinkProgram(prog);

    GLint isLinked = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &isLinked);
    if (isLinked == GL_FALSE)
    {
        LOGE("Could not link program");
    }


    return prog;
}

static void InitCam(JNIEnv* env, jobject surface)
{
    // Prepare surface
    textureWindow = ANativeWindow_fromSurface(env, surface);

    // Prepare request for texture target
    ACameraDevice_createCaptureRequest(cameraDevice, TEMPLATE_PREVIEW, &request);

    // Prepare outputs for session
    ACaptureSessionOutput_create(textureWindow, &textureOutput);
    ACaptureSessionOutputContainer_create(&outputs);
    ACaptureSessionOutputContainer_add(outputs, textureOutput);

// Enable ImageReader example in CMakeLists.txt. This will additionally
// make image data available in imageCallback().
#ifdef WITH_IMAGE_READER
    imageReader = createJpegReader();
    imageWindow = createSurface(imageReader);
    ANativeWindow_acquire(imageWindow);
    ACameraOutputTarget_create(imageWindow, &imageTarget);
    ACaptureRequest_addTarget(request, imageTarget);
    ACaptureSessionOutput_create(imageWindow, &imageOutput);
    ACaptureSessionOutputContainer_add(outputs, imageOutput);
#endif

    // Prepare target surface
    ANativeWindow_acquire(textureWindow);
    ACameraOutputTarget_create(textureWindow, &textureTarget);
    ACaptureRequest_addTarget(request, textureTarget);

    // Create the session
    ACameraDevice_createCaptureSession(cameraDevice, outputs, &sessionStateCallbacks, &textureSession);

    // Start capturing continuously
    ACameraCaptureSession_setRepeatingRequest(textureSession, &captureCallbacks, 1, &request, nullptr);
}

static void InitSurface(JNIEnv* env, jint texId, jobject surface)
{
#if 1
    // Init shaders
    vtxShader = createShader(vertexShaderSrc, GL_VERTEX_SHADER);
    fragShader = createShader(fragmentShaderSrc, GL_FRAGMENT_SHADER);
    prog = createProgram(vtxShader, fragShader);

    // Store attribute and uniform locations
    vtxPosAttrib = glGetAttribLocation(prog, "vertexPosition");
    uvsAttrib = glGetAttribLocation(prog, "uvs");
    mvpMatrix = glGetUniformLocation(prog, "mvp");
    texMatrix = glGetUniformLocation(prog, "texMatrix");
    texSampler = glGetUniformLocation(prog, "texSampler");
    //color = glGetUniformLocation(prog, "color");
    size = glGetUniformLocation(prog, "size");

    // Prepare buffers
    glGenBuffers(2, buf);

    // Set up vertices
    float vertices[] {
            // x, y, z, u, v
            -1, -1, 0, 0, 0,
            -1, 1, 0, 0, 1,
            1, 1, 0, 1, 1,
            1, -1, 0, 1, 0
    };
    glBindBuffer(GL_ARRAY_BUFFER, buf[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    // Set up indices
    GLuint indices[] { 2, 1, 0, 0, 3, 2 };
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_DYNAMIC_DRAW);
#endif
    // We can use the id to bind to GL_TEXTURE_EXTERNAL_OES
    textureId = texId;

    // Prepare the surfaces/targets & initialize session
    InitCam(env, surface);
}


static void InitCamera() {
    sCameraManager = ACameraManager_create();
    assert(sCameraManager);

    ACameraIdList *cameraIds = nullptr;
    ACameraManager_getCameraIdList(sCameraManager, &cameraIds);

    // std::string backId;

    LOGD("found camera count %d", cameraIds->numCameras);

    for (int i = 0; i < cameraIds->numCameras; ++i) {
        const char *id = cameraIds->cameraIds[i];

        ACameraMetadata *metadataObj;
        ACameraManager_getCameraCharacteristics(sCameraManager, id, &metadataObj);

        ACameraMetadata_const_entry lensInfo = {0};
        ACameraMetadata_getConstEntry(metadataObj, ACAMERA_LENS_FACING, &lensInfo);

        ACameraMetadata_const_entry orientationInfo = {0};
        ACameraMetadata_getConstEntry(metadataObj, ACAMERA_SENSOR_ORIENTATION, &orientationInfo);
        auto orient =    orientationInfo.data.i32[0];

        LOGD("Camera orientation %d", orient);
        auto facing = static_cast<acamera_metadata_enum_android_lens_facing_t>(
                lensInfo.data.u8[0]);

        // Found a back-facing camera?
        if (facing == ACAMERA_LENS_FACING_BACK) {
            // backId = id;
            ACameraManager_openCamera(sCameraManager, id, &cameraDeviceCallbacks, &cameraDevice);
            break;
        }
    }

    ACameraManager_deleteCameraIdList(cameraIds);
}

static void ExitCamera()
{
    if (sCameraManager)
    {
        // Stop recording to SurfaceTexture and do some cleanup
        ACameraCaptureSession_stopRepeating(textureSession);
        ACameraCaptureSession_close(textureSession);
        ACaptureSessionOutputContainer_free(outputs);
        ACaptureSessionOutput_free(textureOutput);

        ACameraDevice_close(cameraDevice);
        ACameraManager_delete(sCameraManager);
        sCameraManager = nullptr;
        // Capture request for SurfaceTexture
        ANativeWindow_release(textureWindow);
        ACaptureRequest_free(request);
    }
}

static void DrawFrame(JNIEnv *env,jfloatArray texMatArray)
{
    glViewport(0, 0, width, height);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(prog);
    // Configure main transformations
    float mvp[] = {
            1.0f, 0, 0, 0,
            0, 1.0f, 0, 0,
            0, 0, 1.0f, 0,
            0, 0, 0, 1.0f
    };

    float aspect =    width > height ? float(width)/float(height) : float(height)/float(width);
    float targetAspect =  width > height ? 1920.0f/1080.0f :1080.0f/ 1920.0f;
  //  ortho(mvp, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
    /*
    if (width < height) // portrait
        ortho(mvp, -1.0f, 1.0f, -aspect, aspect, -1.0f, 1.0f);
    else // landscape
        ortho(mvp, -aspect, aspect, -1.0f, 1.0f, -1.0f, 1.0f);
*/

    mat4x4 worldMat;
    mat4x4_identity(worldMat);
    if (width >= height) {
        mat4x4_rotate(worldMat, worldMat, 0, 0, 1, degToRad(90.0f));
    }
   // mat4x4_scale_aniso(worldMat,worldMat,width,height,1.0f);
    mat4x4 projMat;
    mat4x4_identity(projMat);
   // mat4x4_ortho(projMat,-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
/*
    if (V >= A) {
        // wide viewport, use full height
        ortho(-V/A * target_width/2.0f, V/A * target_width/2.0f, -target_height/2.0f, target_height/2.0f, ...);
    } else {
        // tall viewport, use full width
        ortho(-target_width/2.0f, target_width/2.0f, -A/V*target_height/2.0f, A/V*target_height/2.0f, ...);
    }
  */

   float aa =  aspect  / targetAspect;
   // mat4x4_ortho(projMat, -1920 * 0.5f, 1920 * 0.5f,-aa * 1080.0f * 0.5f,  aa *  1080.0f * 0.5f, -1.0f, 1.0f);


   if (width >= height) {
       //LANDSCAPE
      // mat4x4_ortho(projMat, -aa * 1920 * 0.5f, aa * 1920 * 0.5f, -1080.0f * 0.5f, 1080.0f * 0.5f,
             //       -1.0f, 1.0f);
       mat4x4_ortho(projMat, -1.0, 1.0, -1.0f, 1.0f, -1.0f, 1.0f);

   }else{
       //PORTRAIT
    //   mat4x4_ortho(projMat, -1920 * 0.5f, 1920 * 0.5f,-aa * 1080.0f * 0.5f,  aa *  1080.0f * 0.5f, -1.0f, 1.0f);
       mat4x4_ortho(projMat, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
   }


    mat4x4 mvpMat;
    mat4x4_mul(mvpMat,projMat,worldMat);
    glUniformMatrix4fv(mvpMatrix, 1, false, mvpMat[0]);


    // Prepare texture for drawing

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Pass SurfaceTexture transformations to shader
    float* tm = env->GetFloatArrayElements(texMatArray, 0);
    glUniformMatrix4fv(texMatrix, 1, false, tm);
    env->ReleaseFloatArrayElements(texMatArray, tm, 0);

    // Set the SurfaceTexture sampler
    glUniform1i(texSampler, 0);

    // I use red color to mix with camera frames
   // float c[] = { 1, 0, 0, 1 };
   // glUniform4fv(color, 1, (GLfloat*)c);

    // Size of the window is used in fragment shader
    // to split the window
    float sz[2] = {0};
    sz[0] = width;
    sz[1] = height;
    glUniform2fv(size, 1, (GLfloat*)sz);

    // Set up vertices/indices and draw
    glBindBuffer(GL_ARRAY_BUFFER, buf[0]);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf[1]);

    glEnableVertexAttribArray(vtxPosAttrib);
    glVertexAttribPointer(vtxPosAttrib, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (void*)0);

    glEnableVertexAttribArray(uvsAttrib);
    glVertexAttribPointer(uvsAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (void *)(3 * sizeof(float)));

   // glViewport(0, 0, width, height);
  //  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  //  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

}


extern "C" {

JNIEXPORT void JNICALL
Java_com_example_reifydemo_MainActivity_InitCam(JNIEnv *env,
                                                jobject /* this */) {
    InitCamera();
}

JNIEXPORT void JNICALL
Java_com_example_reifydemo_MainActivity_ExitCam(JNIEnv *env,
                                                jobject /* this */) {
    ExitCamera();
}


JNIEXPORT jstring JNICALL
Java_com_example_reifydemo_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}

JNIEXPORT void JNICALL
Java_com_example_reifydemo_CameraGLRender_OnSurfaceCreated(JNIEnv *env, jobject, jint texId, jobject surface)
{
    LOGD("onSurfaceCreated()");
    InitSurface(env, texId, surface);
}

JNIEXPORT void JNICALL
Java_com_example_reifydemo_CameraGLRender_OnSurfaceChanged(JNIEnv *env, jobject, jint w, jint h)
{
    width = w;
    height = h;
}
JNIEXPORT void JNICALL
Java_com_example_reifydemo_CameraGLRender_OnDrawFrame(JNIEnv *env, jobject, jfloatArray texMatArray)
{
   DrawFrame(env,texMatArray);
}

}