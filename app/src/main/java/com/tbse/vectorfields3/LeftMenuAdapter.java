package com.tbse.vectorfields3;

import android.content.Context;
import android.widget.ArrayAdapter;

/**
 * Created by todd on 10/23/16 for the Android Nanodegree capstone project.
 */

public class LeftMenuAdapter extends ArrayAdapter<String> {
    public LeftMenuAdapter(Context context, int resource, int textViewResourceId, String[] objects) {
        super(context, resource, textViewResourceId, objects);
    }
}
