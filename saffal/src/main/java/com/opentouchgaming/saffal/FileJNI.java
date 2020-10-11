package com.opentouchgaming.saffal;

import android.os.Build;
import android.util.Log;

import androidx.annotation.RequiresApi;

public class FileJNI {
    final static String TAG = "FileJNI JAVA";

    // Init the C library, pass in the root path so the path check can be done in C code
    public static native int init(String SAFPath);

    public static int fopen(final String filePath, final String mode) {
        // Log.d(TAG, "fopen file = " + filePath);

        FileSAF fileSAF = new FileSAF(filePath);

        // If not in the SAF area return -1
        if (!UtilsSAF.isInSAFRoot(fileSAF.getPath())) {
            return -1;
        }

        int fd = fileSAF.getFd(mode.contains("w"), true);
        //Log.d(TAG, "fd = " + fd);

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

    /**
     * Try to open the path in SAF and return a string array of the items in the directory
     * @param path
     * @return String array of the items in the directory. Appened with F (is a file) or D (if a directory)
     */
    @RequiresApi(api = Build.VERSION_CODES.LOLLIPOP)
    public static String[] opendir(String path) {

        Log.d(TAG, "opendir path = " + path);

        FileSAF fileSAF = new FileSAF(path);

        if (fileSAF.exists() && fileSAF.isDirectory) {

            FileSAF[] files = fileSAF.listFiles();
            String[] ret = new String[files.length];

            for(int n = 0; n < files.length; n++)
            {
                ret[n] = (files[n].isDirectory() ? "D" : "F") + files[n].getName();
            }

            return ret;
        }
        else {
            return new String[]{};
        }
    }
}
