package com.opentouchgaming.saffal;

import android.net.Uri;
import android.os.Build;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.support.annotation.RequiresApi;
import android.support.v4.provider.DocumentFile;
import android.util.Log;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;

public class FileSAF
{
    final String TAG = "FileSAF";

    // Full path
    String path;

    // is Directory
    boolean isDirectory;

    // Real document;
    DocumentFile docFile;

    // Keep this for NDK. I think the file gets closed if this is GC'ed
    ParcelFileDescriptor parcelFileDescriptor;

    // FD if getFd called
    int fd;

    public FileSAF(String path)
    {
        try
        {
            this.path = new File(path).getCanonicalPath();
        } catch (IOException e)
        {
            e.printStackTrace();
        }
    }

    public String getPath()
    {
        return path;
    }

    public boolean isDirectory()
    {
        updateDocument(true);
        return isDirectory;
    }

    public boolean exists()
    {
        updateDocument(true);
        if (docFile != null)
        {
            return docFile.exists();
        }
        return false;
    }

    public boolean delete()
    {
        updateDocument(true);
        if (docFile != null)
        {
            return docFile.delete();
        }
        return false;
    }

    public int getFd(boolean write)
    {
        updateDocument(true);
        if (docFile != null)
        {
            DBG("URI = " + docFile.getUri().toString());
            try
            {
                parcelFileDescriptor = UtilsSAF.getParcelDescriptor(docFile,write);
                fd = parcelFileDescriptor.getFd();
                return fd;
            } catch (IOException e)
            {
                e.printStackTrace();
            }
        }
        return 0; // Failed
    }

    InputStream getInputStream(DocumentFile docFile)
    {
        updateDocument(true);
        if (docFile != null)
        {
            try
            {
                return UtilsSAF.getInputStream(docFile);
            } catch (FileNotFoundException e)
            {
                e.printStackTrace();
            }
        }
        return null;
    }

    // mkdir is only successful if the parent dir already exists.
    // NOTE it is important not to tell SAF to create a new folder/dir with the same name as as existing file
    // because it will auto rename and make another file (1), (2)..etc
    public boolean mkdir()
    {
        DBG("mkdir: path = " + path);

        DocumentFile newDoc = UtilsSAF.createPath(path);

        boolean successful = (newDoc != null) && newDoc.exists();

        DBG("mkdir: successful = " + successful);

        return (successful);
    }

    public boolean mkdirs()
    {
        DBG("mkdirs: path = " + path);

        DocumentFile newDoc = UtilsSAF.createPaths(path);

        boolean successful = (newDoc != null) && newDoc.exists();

        DBG("mkdirs: successful = " + successful);

        return (successful);
    }

    @RequiresApi(api = Build.VERSION_CODES.LOLLIPOP)
    public boolean renameTo(String newName)
    {
        updateDocument(false);

        if (docFile != null)
        {
            Uri uri = null;
            try
            {
                uri = DocumentsContract.renameDocument(UtilsSAF.getContentResolver(), docFile.getUri(), newName);
            } catch (FileNotFoundException e)
            {
                e.printStackTrace();
            }
            return (uri != null);
        } else
        {
            return false;
        }
    }

    public boolean createNewFile()
    {
        DocumentFile file = UtilsSAF.createFile(path);
        if (file != null)
        {
            updateDocument(true);
            return true;
        } else
        {
            return false;
        }
    }


    public FileSAF[] listFiles()
    {
        FileSAF[] ret = null;

        updateDocument(true);

        if (docFile != null)
        {
            if (docFile.isDirectory())
            {
                // TODO: This is very slow, apparently can be sped up with 'DocumentsContract'
                // EDIT, cant get  'DocumentsContract' to work on anything but the root?!
                DocumentFile[] docFiles = docFile.listFiles();
                if (docFiles != null)
                {
                    ret = new FileSAF[docFiles.length];

                    for (int n = 0; n < docFiles.length; n++)
                    {
                        DocumentFile doc = docFiles[n];
                        Log.d(TAG, "found: " + path + "/" + doc.getName());
                        // Append the file name to the path of this file
                        ret[n] = new FileSAF(path + "/" + doc.getName());
                    }
                }

            }
        }
        return ret;
    }

    private void updateDocument(boolean forceUpdate)
    {
        if (docFile == null || forceUpdate)
        {
            docFile = UtilsSAF.getDocumentFile(path);
            if (docFile != null)
            {
                isDirectory = docFile.isDirectory();
            }
        }
    }

    private void DBG(String str)
    {
        Log.d(TAG, str);
    }
}
