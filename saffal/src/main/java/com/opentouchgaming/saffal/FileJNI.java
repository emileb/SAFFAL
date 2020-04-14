package com.opentouchgaming.saffal;

import android.util.Log;

import java.io.IOException;
import java.util.ArrayList;

public class FileJNI {
    final static String TAG = "FileJNI JAVA";

    // Init the C library, pass in the root path so the path check can be done in C code
    public static native int init(String SAFPath);

    public static int fopen(final String filePath, final String mode) {
        Log.d(TAG, "fopen file = " + filePath);

        FileSAF fileSAF = new FileSAF(filePath);

        // If not in the SAF area return -1 so the NDK code knows this
        if (!UtilsSAF.isInSAFRoot(fileSAF.getPath())) {
            return -1;
        }

        int fd = fileSAF.getFd(mode.contains("w"), true);

        Log.d(TAG, "fd = " + fd);

        return fd;
    }


    public static int mkdir(String path) {
        Log.d(TAG, "mkdir path = " + path);

        FileSAF fileSAF = new FileSAF(path);

        // If not in the SAF area return -1 so the NDK code knows this
        if (!UtilsSAF.isInSAFRoot(fileSAF.getPath())) {
            return -1;
        }

        if (fileSAF.mkdir() == true) // Success
        {
            return 0;
        } else // Failed
        {
            return 1;
        }
    }

    public static int exists(String path) {

        Log.d(TAG, "exists path = " + path);

        FileSAF fileSAF = new FileSAF(path);

        if (fileSAF.exists()) {
            return 1;
        }
        else
        {
            return 0;
        }
    }
}
