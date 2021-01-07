package com.example.reifydemo;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.util.AttributeSet;
import android.util.Size;


public  class CameraSurfaceView extends GLSurfaceView{

    public CameraGLRender cameraGLRender;
    public CameraSurfaceView(Context context) {
        super(context);
    }

public  void SetPreviewImageSize(int w,int h)
{

    cameraGLRender.optimalPreviewSize = new Size(w, h);

}

    public CameraSurfaceView(Context context, AttributeSet attrs) {
        super(context, attrs);
        setEGLContextClientVersion(2);
        cameraGLRender = new CameraGLRender();
        setRenderer(cameraGLRender);
    }
}