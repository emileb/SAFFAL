package com.opentouchgaming.saffal;

import android.util.Log;

import java.io.IOException;
import java.util.ArrayList;

public class FileJNI
{
    final static String TAG = "FileJNI JAVA";

    public static ArrayList<FileSAF> openFiles = new ArrayList<>();

    public static int fopen(final String filePath, final String mode)
    {
        Log.d(TAG,"fopen file = " + filePath);

        FileSAF fileSAF = new FileSAF(filePath);

        // If not in the SAF area return -1 so the NDK code knows this
        if( !UtilsSAF.isInSAFRoot( fileSAF.getPath() ))
        {
            return -1;
        }

        openFiles.add(fileSAF);

        int fd = fileSAF.getFd(mode.contains("w"));

        Log.d(TAG,"fd = " + fd);

        return fd;
    }

    public static int fclose(int fd)
    {
        // Search for FD
        int found = -1;
        for( int n = 0; n < openFiles.size(); n++)
        {
            if( openFiles.get(n).fd == fd )
            {
                found = n;
                break;
            }
        }

        if( found != -1 )
        {
            Log.d(TAG,"fclose closing file " + fd);
            try
            {
                openFiles.get(found).parcelFileDescriptor.close();
            } catch (IOException e)
            {
                e.printStackTrace();
            }
            openFiles.remove(found);
        }
        else
        {
            Log.e(TAG,"ERROR, did not find FD in list, this should not happen!");
        }

        return 0;
    }
}
