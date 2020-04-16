package com.opentouchgaming.saffal;

import android.os.Build;
import android.os.ParcelFileDescriptor;
import android.support.annotation.RequiresApi;
import android.support.v4.provider.DocumentFile;
import android.util.Log;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;

public class FileSAF {
    final String TAG = "FileSAF";

    // Full path
    String path;

    // is Directory
    boolean isDirectory;

    // Real document;
    //DocumentFile docFile;

    DocumentNode documentNode;

    // Keep this for NDK. I think the file gets closed if this is GC'ed
    ParcelFileDescriptor parcelFileDescriptor;

    // FD if getFd called
    int fd;

    public FileSAF(String path) {
        try {
            this.path = new File(path).getCanonicalPath();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    public String getPath() {
        return path;
    }

    @RequiresApi(api = Build.VERSION_CODES.LOLLIPOP)
    public boolean isDirectory() {
        updateDocumentNode(true);
        return isDirectory;
    }

    @RequiresApi(api = Build.VERSION_CODES.LOLLIPOP)
    public boolean exists() {
        updateDocumentNode(true);
        if (documentNode != null) {
            return true;
        }
        return false;
    }

    public boolean delete() {
        /*
        updateDocumentNode(true);
        if (docFile != null) {
            return docFile.delete();
        }
         */
        return false;
    }


    @RequiresApi(api = Build.VERSION_CODES.LOLLIPOP)
    public int getFd(boolean write, boolean detach) {

        updateDocumentNode(true);

        if (documentNode != null) {
            try {
                parcelFileDescriptor = UtilsSAF.getParcelDescriptor(documentNode.documentId, write);
                if (detach)
                    fd = parcelFileDescriptor.detachFd();
                else
                    fd = parcelFileDescriptor.getFd();
                return fd;
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
        return -1; // Failed
    }

    InputStream getInputStream(DocumentFile docFile) {
        /*
        updateDocumentNode(true);
        if (docFile != null) {
            try {
                return UtilsSAF.getInputStream(docFile);
            } catch (FileNotFoundException e) {
                e.printStackTrace();
            }
        }
         */
        return null;
    }

    // mkdir is only successful if the parent dir already exists.
    // NOTE it is important not to tell SAF to create a new folder/dir with the same name as as existing file
    // because it will auto rename and make another file (1), (2)..etc
    public boolean mkdir() {
        /*
        DBG("mkdir: path = " + path);

        DocumentFile newDoc = UtilsSAF.createPath(path);

        boolean successful = (newDoc != null) && newDoc.exists();

        DBG("mkdir: successful = " + successful);

        return (successful);
         */
        return false;
    }

    public boolean mkdirs() {
        /*
        DBG("mkdirs: path = " + path);

        DocumentFile newDoc = UtilsSAF.createPaths(path);

        boolean successful = (newDoc != null) && newDoc.exists();

        DBG("mkdirs: successful = " + successful);

        return (successful);
         */
        return false;
    }

    @RequiresApi(api = Build.VERSION_CODES.LOLLIPOP)
    public boolean renameTo(String newName) {
        /*
        updateDocumentNode(false);

        if (docFile != null) {
            Uri uri = null;
            try {
                uri = DocumentsContract.renameDocument(UtilsSAF.getContentResolver(), docFile.getUri(), newName);
            } catch (FileNotFoundException e) {
                e.printStackTrace();
            }
            return (uri != null);
        } else {
            return false;
        }
         */
        return false;
    }

    public boolean createNewFile() {
        /*
        DocumentFile file = UtilsSAF.createFile(path);
        if (file != null) {
            updateDocumentNode(true);
            return true;
        } else {
            return false;
        }
         */
        return false;
    }


    @RequiresApi(api = Build.VERSION_CODES.LOLLIPOP)
    public File getRealFile(String path)
    {
        File file = null;

        if(exists())
        {
            int fd = getFd(false,true);

            String linkFileName = UtilsSAF.getFdPath(fd);

            if(linkFileName != null  && linkFileName.length() > 0) {
                file = new File(linkFileName);
            }

            DBG("getRealFile: Real File = " + linkFileName );
        }

        return file;
    }

    @RequiresApi(api = Build.VERSION_CODES.LOLLIPOP)
    public FileSAF[] listFiles() {

        DBG("listFiles: path = " + getPath());

        updateDocumentNode(true);

        ArrayList<FileSAF> files = new ArrayList<>();

        if (documentNode != null) {
            if (documentNode.isDirectory) {
                {
                    ArrayList<DocumentNode> nodes = documentNode.getChildren();

                    // For each valid document, create a new FileSAF and return
                    for(DocumentNode node : nodes)
                    {
                        files.add(new FileSAF(path + "/" + node.name));
                    }
                }
            }
        }
        return files.toArray(new FileSAF[files.size()]);
    }

    @RequiresApi(api = Build.VERSION_CODES.LOLLIPOP)
    private void updateDocumentNode(boolean forceUpdate) {
        if(documentNode == null || forceUpdate) {
            String documentPath = UtilsSAF.getDocumentPath(path);

            documentNode = DocumentNode.findDocumentNode(UtilsSAF.documentRoot, documentPath);

            if (documentNode != null) {
                DBG("FileSAF (" + path + ") found Documet: " + documentNode.documentId);
                isDirectory = documentNode.isDirectory;
            }
        }
    }

    private void DBG(String str) {
        Log.d(TAG, str);
    }
}
