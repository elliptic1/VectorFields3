package com.tbse.vectorfields3;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.util.AttributeSet;
import android.view.MotionEvent;

public class DemoGLSurfaceView extends GLSurfaceView {
    public DemoGLSurfaceView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mRenderer = new DemoRenderer();
        setRenderer(mRenderer);
    }

    public DemoGLSurfaceView(Context context) {
        super(context);
        mRenderer = new DemoRenderer();
        setRenderer(mRenderer);
    }

    public boolean onTouchEvent(final MotionEvent event) {
        if (event.getAction() == MotionEvent.ACTION_DOWN
                || event.getAction() == MotionEvent.ACTION_MOVE) {
            nativeTouchEvent(event.getX(), event.getY());
        }
        return true;
    }

    @Override
    public void onPause() {
        super.onPause();
        nativePause();
    }

    @Override
    public void onResume() {
        super.onResume();
        nativeResume();
    }


    DemoRenderer mRenderer;

    private static native void nativePause();

    private static native void nativeResume();

    private static native void nativeTogglePauseResume();

    private static native void nativeTouchEvent(float x, float y);
}
