package com.opentouchgaming.saffal;

import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.provider.DocumentsContract;
import android.support.annotation.RequiresApi;
import android.util.Log;

import java.util.ArrayList;
import java.util.List;

public class DocumentNode {

    static String TAG = "DocumentNode";

    public String name;
    public String documentId;
    public boolean exists;
    public boolean isDirectory;
    public DocumentNode parent;

    private List<DocumentNode> children;

    @RequiresApi(api = Build.VERSION_CODES.LOLLIPOP)
    public DocumentNode findChild(String name) {

        DocumentNode node = null;

        findChildren();

        if (children != null) {
            for (DocumentNode n : children) {
                if (n.name.contentEquals(name)) {
                    node = n;
                    break;
                }
            }
        }
        return node;
    }

    @RequiresApi(api = Build.VERSION_CODES.LOLLIPOP)
    public ArrayList<DocumentNode> getChildren()
    {
        findChildren();

        ArrayList<DocumentNode> ret = new ArrayList<>();

        if(children!= null)
            ret.addAll(children);

        return ret;
    }

    @RequiresApi(api = Build.VERSION_CODES.LOLLIPOP)
    public void findChildren() {
        if (isDirectory) {
            if (children == null) // Check is already been scanned, this is allows caching of the files
            {
                children = new ArrayList<>();

                Uri childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(UtilsSAF.getTreeRoot().uri, documentId);

                Cursor cursor = UtilsSAF.context.getContentResolver().query(childrenUri, new String[]{
                        DocumentsContract.Document.COLUMN_DISPLAY_NAME, DocumentsContract.Document.COLUMN_MIME_TYPE, DocumentsContract.Document.COLUMN_DOCUMENT_ID}, null, null, null);
                try {
                    while (cursor.moveToNext()) {

                        String displayName = cursor.getString(0);
                        String mimeType = cursor.getString(1);
                        String documentId = cursor.getString(2);

                        DBG("findChildren: child=" + displayName + ", docId=" + documentId);

                        DocumentNode newNode = new DocumentNode();
                        newNode.name = displayName;
                        newNode.exists = true;
                        newNode.isDirectory = DocumentsContract.Document.MIME_TYPE_DIR.equals(mimeType);
                        newNode.documentId = documentId;
                        newNode.parent = this;

                        children.add(newNode);
                    }
                } finally {
                    try {
                        cursor.close();
                    } catch (RuntimeException rethrown) {
                        throw rethrown;
                    } catch (Exception ignored) {
                    }
                }
            }
        } else {
            DBG("DocumentNode: ERROR, tried to findChildren of non direcotry");
        }
    }

    /**
     * Traverse a tree to find a particular DocumentNode. Path must look like:  folder1/folder2/file, or folder1/folder2/folder3
     *
     * @return The node if it exists, otherwise null;
     */
    @RequiresApi(api = Build.VERSION_CODES.LOLLIPOP)
    public static DocumentNode findDocumentNode(DocumentNode rootNode, String documentPath) {

        DocumentNode node = null;

        if (documentPath != null) {

            // Split path into parts
            String[] parts = documentPath.split("\\/", -1);

            node = rootNode;
            for (String part : parts) {
                if(part.length() > 0) { // part will be an empty string if documentPath was an empty string
                    DBG("findDocumentNode: part = " + part);

                    if (node == null)
                        break;

                    node = node.findChild(part);
                }
            }
        }

        return node;
    }

    private static void DBG(String str) {
        Log.d(TAG, str);
    }
}
