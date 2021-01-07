package com.example.reifydemo;

import android.content.Context;
import android.graphics.SurfaceTexture;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.opengl.GLES11Ext;
import android.opengl.GLES20;
import android.opengl.GLSurfaceView;
import android.util.Log;
import android.util.Size;
import android.view.Surface;
import android.app.Activity;


import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public class CameraGLRender implements GLSurfaceView.Renderer {

    SurfaceTexture surfaceTexture;
    Object lock;
    volatile boolean frameAvailable = false;
    float[] matr = new float[16];
    public Size optimalPreviewSize = new Size(640,480);
    public   Surface surface;
    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        int[] textures = new int  [1];
        lock = new Object();
        GLES20.glGenTextures(1,textures,0);
        GLES20.glBindTexture( GLES11Ext.GL_TEXTURE_EXTERNAL_OES,textures[0]);
        surfaceTexture = new SurfaceTexture(textures[0]);
        surfaceTexture.setOnFrameAvailableListener(new SurfaceTexture.OnFrameAvailableListener() {
            @Override
            public void onFrameAvailable(SurfaceTexture surfaceTexture) {
                synchronized (lock){
                    frameAvailable = true;
                }
            }
        });
        surface =  new Surface(surfaceTexture);
      surfaceTexture.setDefaultBufferSize(optimalPreviewSize.getWidth(),optimalPreviewSize.getHeight());

      OnSurfaceCreated(textures[0],surface);//call C++
    }



    @Override
    public void onSurfaceChanged(GL10 gl, int width, int height) {
        OnSurfaceChanged(width,height); //call C++
    }

    @Override
    public void onDrawFrame(GL10 gl)
    {
     synchronized (lock)
     {
         if (frameAvailable)
         {
             Log.d("DD","Updating frame");
             surfaceTexture.updateTexImage();
             surfaceTexture.getTransformMatrix(matr);
             frameAvailable = false;
         }
     }
       OnDrawFrame(matr);//C++
    }

     public native void OnSurfaceCreated(int texId,Surface surface);
     public native void OnSurfaceChanged(int width,int height);
     public native void OnDrawFrame(float[] matr);

}


