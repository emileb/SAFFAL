package com.opentouchgaming.saftest;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.DocumentsContract;
import android.util.Log;

import com.opentouchgaming.saffal.FileSAF;
import com.opentouchgaming.saffal.UtilsSAF;
import com.opentouchgaming.saftest.databinding.ActivityMainBinding;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;

/**
 * Runs the native SAFFAL regression suite (saftest.cpp) under the SAFFAL settings of each OpenTouch process.
 * adb: am start -n com.opentouchgaming.saftest/.MainActivity --ez autorun true, then logcat -s SAFTEST.
 */
public class MainActivity extends Activity
{
    static final String TAG = "SAFTEST";
    static final int PICK_FOLDER = 1;

    // launcher: no native caches; game: native caches (game process on Android 11+); game_prefetch: plus prefetch
    static final String[] MODES = {"launcher", "game", "game_prefetch"};

    ActivityMainBinding binding;
    volatile boolean running;

    static native String runSuite(String realBase, String safBase, String mode);

    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        UtilsSAF.setContext(getApplicationContext(), false);
        System.loadLibrary("saftest");
        UtilsSAF.loadTreeRoots(this);

        binding.pick.setOnClickListener(v -> UtilsSAF.openDocumentTree(this, PICK_FOLDER));
        binding.run.setOnClickListener(v -> runTests());
        updateStatus();

        if (getIntent().getBooleanExtra("autorun", false))
            runTests();
    }

    void updateStatus()
    {
        UtilsSAF.TreeRoot root = UtilsSAF.getTreeRoot();
        binding.status.setText(root == null ? "Pick a SAF folder first" : "SAF folder: " + root.rootPath);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data)
    {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != PICK_FOLDER || resultCode != RESULT_OK || data == null)
            return;

        Uri treeUri = data.getData();
        String rootDocumentId = DocumentsContract.getTreeDocumentId(treeUri);

        // Same root path naming as AndroidCore's ScopedStorage
        String[] split = rootDocumentId.split(":", -1);
        String root = split[0].contentEquals("primary") ? "/[internal]" : "/[SD-Card]";
        if (split.length > 1 && split[1].length() > 0)
            root = root + "/" + split[1];

        getContentResolver().takePersistableUriPermission(treeUri, Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        UtilsSAF.setTreeRoot(new UtilsSAF.TreeRoot(treeUri, root, rootDocumentId));
        UtilsSAF.saveTreeRoots(this);
        updateStatus();
    }

    void runTests()
    {
        if (running)
            return;

        if (UtilsSAF.getTreeRoot() == null)
        {
            binding.results.setText("Pick a SAF folder first");
            Log.e(TAG, "SAFTEST ERROR no SAF folder picked");
            return;
        }

        running = true;
        binding.results.setText("Running...\n");

        new Thread(() ->
        {
            // New folder names each run, so nothing SAFFAL cached in an earlier run can match
            String run = "run" + System.currentTimeMillis();
            File realTop = new File(getExternalFilesDir(null), "saftest");
            String safTop = UtilsSAF.getTreeRoot().rootPath + "/saftest";

            deleteTree(realTop);
            FileSAF oldSaf = new FileSAF(safTop);
            if (oldSaf.exists())
                oldSaf.delete();

            StringBuilder report = new StringBuilder();
            for (String mode : MODES)
            {
                // Same calls as SDLActivity
                UtilsSAF.setContext(getApplicationContext(), !mode.equals("launcher"));
                UtilsSAF.setPrefetch(mode.equals("game_prefetch"));
                UtilsSAF.loadTreeRoots(getApplicationContext());

                File realBase = new File(realTop, run + "/" + mode);
                realBase.mkdirs();
                String safBase = safTop + "/" + run + "/" + mode;
                new FileSAF(safBase).mkdirs();

                report.append(runSuite(realBase.getPath(), safBase, mode)).append('\n');
                final String text = report.toString();
                runOnUiThread(() -> binding.results.setText(text));
            }

            File out = new File(getExternalFilesDir(null), "saftest_report.txt");
            try (FileOutputStream os = new FileOutputStream(out))
            {
                os.write(report.toString().getBytes());
            }
            catch (IOException e)
            {
                Log.e(TAG, "Could not write " + out, e);
            }

            Log.i(TAG, "SAFTEST FINISHED, report in " + out);
            running = false;
        }).start();
    }

    static void deleteTree(File f)
    {
        File[] children = f.listFiles();
        if (children != null)
        {
            for (File c : children)
                deleteTree(c);
        }
        f.delete();
    }
}
