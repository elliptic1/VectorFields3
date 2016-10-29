package com.tbse.vectorfields3;

import android.util.Log;
import android.view.View;
import android.widget.AdapterView;

/**
 * Created by todd on 10/23/16 for the Android Nanodegree capstone project.
 */

public class LeftMenuOnItemClickListener implements AdapterView.OnItemClickListener {
    @Override
    public void onItemClick(AdapterView<?> adapterView, View view, int i, long l) {
        Log.d("vf", "item clicked " + i + " - " + l);
    }
}
