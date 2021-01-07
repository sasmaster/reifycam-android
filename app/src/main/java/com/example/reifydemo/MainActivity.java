package com.example.reifydemo;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;

import android.Manifest;
import android.content.Context;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.PixelFormat;
import android.graphics.RectF;
import android.graphics.SurfaceTexture;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.media.MediaActionSound;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.HandlerThread;
import android.provider.MediaStore;
import android.util.Log;
import android.util.Size;
import android.view.PixelCopy;
import android.view.Surface;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;
import java.io.FileOutputStream;
import java.text.SimpleDateFormat;
import java.util.Date;

public class MainActivity extends AppCompatActivity {
    private static final int MY_CAMERA_REQUEST_CODE = 666;

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("native-lib");
    }

    private CameraSurfaceView cameraSurfaceView;
    private Button camTakePicButton;
    private MediaActionSound sound;
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
//wil be used to play shutter sound when pic taken
        sound = new MediaActionSound();


        //Force landscape,this one is not aware of the landscape variation of the widget so
        //if landscape is forces via it,the default (portrait )landscape is set
        // setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        //hide tytle bar
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        this.getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);

        setContentView(R.layout.activity_main);
        camTakePicButton = findViewById(R.id.takePhotoButton);
        camTakePicButton.setOnClickListener(new View.OnClickListener() {

            @Override
            public void onClick(View view) {
                TextView text = findViewById(R.id.sample_text);
                takePhoto();
                sound.play(MediaActionSound.SHUTTER_CLICK);
                text.setText("Picture taken!");
            }


        });
        cameraSurfaceView = (CameraSurfaceView) findViewById(R.id.camSrufaceView);
        // cameraSurfaceView.setZOrderOnTop(true);
        //cameraSurfaceView.setZOrderMediaOverlay(true);
        cameraSurfaceView.getHolder().setFormat(PixelFormat.TRANSPARENT);
        Size optimalSize = new Size(640, 360);
        try {
            optimalSize = GetOptimalPreviewSize();
        } catch (CameraAccessException e) {
            e.printStackTrace();
        }
        cameraSurfaceView.SetPreviewImageSize(optimalSize.getWidth(), optimalSize.getHeight());
        try {
            if (!IsNativeCameraSupported()) {
                Toast.makeText(this,
                        "Native cam API is not supported by this device", Toast.LENGTH_LONG).show();
                finish();
            }
        } catch (CameraAccessException e) {
            e.printStackTrace();
            finish();
        }


        //  int rotation =  getWindowManager().getDefaultDisplay().getRotation();
        int orient = getResources().getConfiguration().orientation;
        if (orient == Configuration.ORIENTATION_LANDSCAPE) {

            cameraSurfaceView.setRotation(0);

            Log.d("Daiya", "ORIENTATION_LANDSCAPE");

        } else if (orient == Configuration.ORIENTATION_PORTRAIT) {


            Log.d("Daiya", "ORIENTATION_PORTRAIT");
        }
        Log.d("df", "dfdf" + orient);

        int rotation = getWindowManager().getDefaultDisplay().getRotation();


        if (rotation == Surface.ROTATION_0 || rotation == Surface.ROTATION_270) {
            //  cameraSurfaceView.setRotation(270);//90*(rotation-2));
        } else if (rotation == Surface.ROTATION_90 || rotation == Surface.ROTATION_270) {

            //   cameraSurfaceView.setRotation(90*(rotation-2));
            // matrix.postScale(scale,scale,centerX,centery);
            //  matrix.postRotate(90*(rotation-2),centerX,centery);


        }

        // Example of a call to a native method
        // TextView tv = findViewById(R.id.sample_text);
        // tv.setText(stringFromJNI());
    }



    private void SaveImage(Bitmap finalBitmap,String fileName) {
        if (isExternalStorageWritable() == false) {
            Toast.makeText(this,
                    "external storage is unaccessable", Toast.LENGTH_LONG).show();
             return;
        }
        String root = Environment.getExternalStorageDirectory().toString();
        File myDir = new File(root + "/saved_images");
        myDir.mkdirs();


        File file = new File(myDir, fileName);
        if (file.exists()) file.delete ();
        try {
            FileOutputStream out = new FileOutputStream(file);
            finalBitmap.compress(Bitmap.CompressFormat.JPEG, 100, out);
            out.flush();
            out.close();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    /* Checks if external storage is available for read and write */
    public boolean isExternalStorageWritable() {
        String state = Environment.getExternalStorageState();
        if (Environment.MEDIA_MOUNTED.equals(state)) {
            return true;
        }
        return false;
    }
    private void takePhoto() {

        // Create a bitmap the size of the scene view.
        final Bitmap bitmap = Bitmap.createBitmap(cameraSurfaceView.getWidth(), cameraSurfaceView.getHeight(),
                Bitmap.Config.ARGB_8888);



        // Create a handler thread to offload the processing of the image.
        final HandlerThread handlerThread = new HandlerThread("PixelCopier");
        handlerThread.start();
        // Make the request to copy.

        PixelCopy.request(cameraSurfaceView, bitmap, (copyResult) -> {
            if (copyResult == PixelCopy.SUCCESS) {

                String name = String.valueOf(System.currentTimeMillis() + ".jpg");
            //    imageFile = ScreenshotUtils.store(bitmap,name);
              //  SaveImage(bitmap,name);
                MediaStore.Images.Media.insertImage(getContentResolver(), bitmap, name , "ReifyTestIMages");
            } else {
                Toast toast = Toast.makeText(this,
                        "Failed to copyPixels: " + copyResult, Toast.LENGTH_LONG);
                toast.show();
            }
            handlerThread.quitSafely();
        }, new Handler(handlerThread.getLooper()));
    }


    @Override
    public void onConfigurationChanged(@NonNull Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        // Checks the orientation of the screen
        if (newConfig.orientation == Configuration.ORIENTATION_LANDSCAPE) {


            Log.d("Daiya", "ORIENTATION_LANDSCAPE");

        } else if (newConfig.orientation == Configuration.ORIENTATION_PORTRAIT) {


            Log.d("Daiya", "ORIENTATION_PORTRAIT");
        }
    }

    @Override
    protected void onResume() {
        super.onResume();

        String camPermission = Manifest.permission.CAMERA;
        if (checkSelfPermission(camPermission) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(new String[]{Manifest.permission.CAMERA}, MY_CAMERA_REQUEST_CODE);
        }
        InitCam();

        cameraSurfaceView.onResume();
    }

    @Override
    protected void onPause() {
        super.onPause();
        cameraSurfaceView.onPause();
        ExitCam();
    }

    private Size GetOptimalPreviewSize() throws CameraAccessException {

        CameraManager cameraManager = (CameraManager) getSystemService(Context.CAMERA_SERVICE);
        String[] idList = cameraManager.getCameraIdList();
        for (int i = 0; i < idList.length; i++) {
            String id = idList[i];
            CameraCharacteristics characteristics = cameraManager.getCameraCharacteristics(id);
            int hwLEvel = characteristics.get(CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL);
            if (hwLEvel != CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL_LEGACY) {

                StreamConfigurationMap map = characteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
                Size[] availableSizes = map.getOutputSizes(SurfaceTexture.class);
                for (int c = 0; c < availableSizes.length; ++c) {
                    Size currSize = availableSizes[c];
                    if (currSize.getWidth() > 1280 && currSize.getWidth() < 2000) {
                        //select HD
                        return availableSizes[c];
                    }

                }

            }
        }
        return new Size(640, 360);
    }

    protected boolean IsNativeCameraSupported() throws CameraAccessException {
        CameraManager cameraManager = (CameraManager) getSystemService(Context.CAMERA_SERVICE);
        String[] idList = cameraManager.getCameraIdList();
        for (int i = 0; i < idList.length; i++) {
            String id = idList[i];
            CameraCharacteristics characteristics = cameraManager.getCameraCharacteristics(id);
            int hwLEvel = characteristics.get(CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL);
            if (hwLEvel != CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL_LEGACY) {
                return true;
            }
        }
        return false;
    }

    /**
     * Inits back camera in C++
     */
    public native void InitCam();

    public native void ExitCam();

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    public native String stringFromJNI();
}