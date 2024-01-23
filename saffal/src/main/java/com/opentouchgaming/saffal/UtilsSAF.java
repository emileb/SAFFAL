package com.opentouchgaming.saffal;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Build;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.system.Os;
import android.text.TextUtils;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;
import androidx.documentfile.provider.DocumentFile;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;

public class UtilsSAF
{
    static String TAG = "UtilsSAF";

    static Context context;

    static int cacheNativeFs = 0;
    static boolean CASE_INSENSITIVE = true;

    /*
        TODO: Only one tree root possible at the moment, this should be extended to a list so we can have
        multiple roots E.G internal phone flash, and SD Card
    */
    static TreeRoot treeRoot;


    // The root node of the selected SAF folder
    static DocumentNode documentRoot;

    /*
        Holds the URI returned from ACTION_OPEN_DOCUMENT_TREE (important!)
        Also the File system 'root' this should point to E.G could be '/storage/emulated/0' for internal files
     */
    public static class TreeRoot
    {
        public Uri uri;
        public String rootPath;
        public String rootDocumentId;

        public TreeRoot(Uri uri, String rootPath, String rootDocumentId)
        {
            this.uri = uri;
            this.rootPath = rootPath;
            this.rootDocumentId = rootDocumentId;
        }
    }

    /**
     * Set a Context so all operations don't need to pass in a new one
     *
     * @param ctx the Context.
     */
    public static void setContext(@NonNull Context ctx, boolean cacheNativeFs)
    {
        UtilsSAF.context = ctx;
        UtilsSAF.cacheNativeFs = cacheNativeFs ? 1 : 0;
        // Load C library
        System.loadLibrary("saffal");
    }

    /**
     * Set a new URI and root path for the URI
     *
     * @param treeRoot the uri and root path.
     */
    public static void setTreeRoot(@NonNull TreeRoot treeRoot)
    {

        UtilsSAF.treeRoot = treeRoot;

        documentRoot = new DocumentNode();
        documentRoot.name = "root";
        documentRoot.isDirectory = true;
        documentRoot.documentId = treeRoot.rootDocumentId;

        // Update the C library with the root path
        FileJNI.init(treeRoot.rootPath, cacheNativeFs);
    }

    /**
     * Get the current tree root
     *
     * @return treeRoot;
     */
    public static TreeRoot getTreeRoot()
    {
        return treeRoot;
    }

    /**
     * Get ContentResolver
     *
     * @return ContentResolver
     */
    public static ContentResolver getContentResolver()
    {
        return context.getContentResolver();
    }

    /**
     * Launch the Select Document screen. You should give some pictures about how to select the internal storage
     *
     * @param activity Your Activity
     * @param code     Code return on onActivityResult
     */
    public static void openDocumentTree(@NonNull Activity activity, int code)
    {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION |
                        Intent.FLAG_GRANT_WRITE_URI_PERMISSION |
                        Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION |
                        Intent.FLAG_GRANT_PREFIX_URI_PERMISSION);

        activity.startActivityForResult(intent, code);
    }

    /**
     * Save the currently set URI and root path so loadTreeRoot can work
     *
     * @param ctx A Context
     */
    public static void saveTreeRoot(Context ctx)
    {
        if (treeRoot != null && treeRoot.uri != null && treeRoot.rootPath != null && treeRoot.rootDocumentId != null)
        {
            SharedPreferences prefs = ctx.getSharedPreferences("utilsSAF", 0);
            SharedPreferences.Editor prefsEdit = prefs.edit();
            prefsEdit.putString("uri", treeRoot.uri.toString());
            prefsEdit.putString("rootPath", treeRoot.rootPath);
            prefsEdit.putString("rootDocumentId", treeRoot.rootDocumentId);
            prefsEdit.commit();
        }
        else
        {
            DBG("saveTreeRoot: ERROR, treeRoot not complete");
        }
    }

    /**
     * Load the last save URI and root
     *
     * @param ctx A Context
     */
    public static boolean loadTreeRoot(Context ctx)
    {
        try
        {
            SharedPreferences prefs = ctx.getSharedPreferences("utilsSAF", 0);

            String url = prefs.getString("uri", null);
            if (url != null)
            {
                Uri treeUri = null;
                treeUri = Uri.parse(url);
                String rootPath = prefs.getString("rootPath", null);
                String rootDocumentId = prefs.getString("rootDocumentId", null);
                if (rootPath != null && rootDocumentId != null)
                {
                    setTreeRoot(new TreeRoot(treeUri, rootPath, rootDocumentId));
                    return true;
                }
            }
        }
        catch (IllegalArgumentException e)
        {
            e.printStackTrace();
        }
        return false;
    }

    /**
     * Returns true when SAF files are ready to be accessed. Note does not tell you the correct URI was chosen by the user.
     *
     * @return True if ready
     */
    public static boolean ready()
    {
        if (treeRoot != null && treeRoot.uri != null && treeRoot.rootPath != null && treeRoot.rootDocumentId != null && context != null)
            return true;
        else
            return false;
    }

    /**
     * Returns true if the path is in the SAF controlled space
     *
     * @return True if in SAF space
     */
    public static boolean isInSAFRoot(String path)
    {
        if (ready())
            return path.startsWith(treeRoot.rootPath);
        else
            return false;
    }

    /**
     * Returns true if the path is at the start of the SAF root
     *
     * @return True if in SAF space
     */
    public static boolean isRootOfSAFRoot(String path)
    {
        if (ready())
        {
            String[] inputPathParts = path.split("/");
            String[] startStringParts = treeRoot.rootPath.split("/");
            if (inputPathParts.length > startStringParts.length)
            {
                return false;
            }
            for (int i = 0; i < inputPathParts.length; i++)
            {
                if (!inputPathParts[i].equals(startStringParts[i]))
                {
                    return false;
                }
            }
            return true;
        }
        return false;
    }

    // Found at: https://stackoverflow.com/questions/30546441/android-open-file-with-intent-chooser-from-uri-obtained-by-storage-access-frame
    public static String getFdPath(int fd)
    {

        //if(true)
        //    return "/proc/self/fd/" + fd;

        final String resolved;

        try
        {
            final File procfsFdFile = new File("/proc/self/fd/" + fd);

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP)
            {
                // Returned name may be empty or "pipe:", "socket:", "(deleted)" etc.
                resolved = Os.readlink(procfsFdFile.getAbsolutePath());
            }
            else
            {
                // Returned name is usually valid or empty, but may start from
                // funny prefix if the file does not have a name
                resolved = procfsFdFile.getCanonicalPath();
            }

            if (TextUtils.isEmpty(resolved) || resolved.charAt(0) != '/' || resolved.startsWith("/proc/") || resolved.startsWith("/fd/"))
                return null;
        }
        catch (IOException ioe)
        {
            // This exception means, that given file DID have some name, but it is
            // too long, some of symlinks in the path were broken or, most
            // likely, one of it's directories is inaccessible for reading.
            // Either way, it is almost certainly not a pipe.
            return "";
        }
        catch (Exception errnoe)
        {
            // Actually ErrnoException, but base type avoids VerifyError on old versions
            // This exception should be VERY rare and means, that the descriptor
            // was made unavailable by some Unix magic.
            return null;
        }

        return resolved;
    }


    // public static ArrayList<FileSAF> fileskeep = new ArrayList<>();

    /**
     * Returns a REAL File, even for files in SAF! NOTE, the file name and path of the file in SAF may be incorrect
     *
     * @return File or null
     */
    //@RequiresApi(api = Build.VERSION_CODES.LOLLIPOP)
    public static File getRealFile(String path)
    {
        DBG("getRealFile: " + path);
        File file = null;
        if (ready() && isInSAFRoot(path))
        {
            FileSAF fileSAF = new FileSAF(path);
            // fileskeep.add(fileSAF);
            if (fileSAF.exists())
            {
                file = fileSAF.getRealFile(path);
            }
        }
        else
        {
            file = new File(path);
        }

        return file;
    }

    static InputStream getInputStream(DocumentFile docFile) throws FileNotFoundException
    {
        return context.getContentResolver().openInputStream(docFile.getUri());
    }

    @RequiresApi(api = Build.VERSION_CODES.LOLLIPOP)
    static ParcelFileDescriptor getParcelDescriptor(String documentId, boolean write) throws IOException
    {
        //DBG("getFd read = " + docFile.canRead() + " write = " + docFile.canWrite() + " name = " + docFile.getName());
        Uri childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(UtilsSAF.getTreeRoot().uri, documentId);

        // NOTE! If we are writing we ALWAYS truncate the file (rwt), this means append won't work, will fix if needed
        ParcelFileDescriptor filePfd = context.getContentResolver().openFileDescriptor(childrenUri, write ? "rwt" : "r");

        return filePfd;
    }

    @RequiresApi(api = Build.VERSION_CODES.LOLLIPOP)
    static boolean deleteDocument(String documentId) throws IOException
    {
        Uri uri = DocumentsContract.buildChildDocumentsUriUsingTree(UtilsSAF.getTreeRoot().uri, documentId);

       return DocumentsContract.deleteDocument(context.getContentResolver(), uri);
    }

    public static String getDocumentPath(String fullPath)
    {
        if (!fullPath.startsWith(treeRoot.rootPath))
        {
            DBG("getDocumentPath: ERROR, filePath (" + fullPath + ") must start with the rootPath (" + treeRoot.rootPath + ")");
            return null;
        }

        if (fullPath.length() > treeRoot.rootPath.length())
        {
            return fullPath.substring(treeRoot.rootPath.length() + 1); // Remove the first "/"
        }
        else
        {
            return "";
        }
    }

    public static String[] getParts(String fullPath)
    {
        String childPath = getDocumentPath(fullPath);

        if (childPath.contentEquals(""))
            return new String[0];
        else
            return childPath.split("\\/", -1);

    }


    private static void DBG(String str)
    {
        Log.d(TAG, str);
    }
}
