package com.opentouchgaming.saffal;

import android.os.Build;
import android.os.ParcelFileDescriptor;

import androidx.annotation.RequiresApi;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Locale;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.FutureTask;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Opens the other files of a SAF folder in the background once one of them is read.
 * Each openFileDescriptor is ~17 ms on the DocumentsProvider but they run well in parallel
 * (8 threads: ~2.4 ms each), so games that read hundreds of loose files at startup load much faster.
 * Unused fds are bounded: at most MAX_PREFETCHED, never past half the process fd limit, and the
 * oldest folder's unused fds are closed to make room for a new folder.
 */
class SAFPrefetch
{
    private static final int THREADS = 8;
    private static final int MIN_FILES = 8;          // Small folders are not worth it
    private static final int MAX_PREFETCHED = 1024;

    static volatile boolean enabled = false; // Opt-in, see UtilsSAF.setPrefetch

    private static final ExecutorService pool = Executors.newFixedThreadPool(THREADS, r ->
    {
        Thread t = new Thread(r, "SAFPrefetch");
        t.setDaemon(true);
        return t;
    });

    // Lower-cased path -> pending or finished open (fd, or -1)
    private static final ConcurrentHashMap<String, FutureTask<Integer>> prefetched = new ConcurrentHashMap<>();
    private static final Set<String> foldersDone = Collections.newSetFromMap(new ConcurrentHashMap<>()); // newKeySet() is API 24+
    private static final AtomicInteger count = new AtomicInteger();

    // Keys queued per folder, oldest folder first, so unused fds can be reclaimed
    private static final ArrayDeque<ArrayList<String>> folderKeys = new ArrayDeque<>();

    private static int fdLimit = -1;

    private static String key(String path)
    {
        return path.toLowerCase(Locale.ROOT);
    }

    // Returns a prefetched read-only fd for the path, or -1. The caller owns the fd.
    static int take(String path)
    {
        FutureTask<Integer> task = prefetched.remove(key(path));
        if (task == null)
            return -1;

        count.decrementAndGet();
        task.run(); // Opens it here if no worker has started it yet
        try
        {
            return task.get();
        }
        catch (Exception e)
        {
            return -1;
        }
    }

    // A path is about to be written, renamed or deleted, or its slot is reclaimed: close any fd opened for it
    static void invalidate(String path)
    {
        FutureTask<Integer> task = prefetched.remove(key(path));
        if (task == null)
            return;

        count.decrementAndGet();
        task.run(); // Finish it rather than cancel, so an fd being opened right now is not leaked
        try
        {
            int fd = task.get();
            if (fd > 0)
                ParcelFileDescriptor.adoptFd(fd).close();
        }
        catch (Exception e)
        {
            // Failed to open, nothing to close
        }
    }

    // Called after a read-only open of a file: queue opens of the rest of its folder
    @RequiresApi(api = Build.VERSION_CODES.LOLLIPOP)
    static synchronized void fileOpened(FileSAF file)
    {
        if (!enabled)
            return;

        String folder = file.getParent();
        if (folder == null || !foldersDone.add(key(folder)))
            return;

        DocumentNode folderNode = UtilsSAF.findDocumentNode(folder);
        if (folderNode == null || !folderNode.isDirectory)
            return;

        ArrayList<DocumentNode> children = folderNode.getChildren();
        if (children.size() < MIN_FILES)
            return;

        // Make room by closing what earlier folders never used
        int wanted = Math.min(children.size(), MAX_PREFETCHED);
        while (!folderKeys.isEmpty() && budget() < wanted)
        {
            for (String k : folderKeys.removeFirst())
                invalidate(k);
        }

        int budget = budget();
        String opened = key(file.getPath());
        ArrayList<String> keys = new ArrayList<>();

        for (DocumentNode node : children)
        {
            if (keys.size() >= budget)
                break;

            if (node.isDirectory)
                continue;

            String k = key(folder + "/" + node.name);
            if (k.equals(opened))
                continue;

            FutureTask<Integer> task = new FutureTask<>(() ->
            {
                try
                {
                    return UtilsSAF.getParcelDescriptor(node.documentId, node.treeUri, false, false).detachFd();
                }
                catch (Exception e)
                {
                    return -1;
                }
            });

            if (prefetched.putIfAbsent(k, task) == null)
            {
                count.incrementAndGet();
                keys.add(k);
                pool.execute(task);
            }
        }

        if (!keys.isEmpty())
            folderKeys.addLast(keys);
    }

    // How many more fds may be prefetched: within MAX_PREFETCHED and keeping the process under half its fd limit
    private static int budget()
    {
        int left = MAX_PREFETCHED - count.get();

        String[] open = new File("/proc/self/fd").list();
        if (open == null)
            return 0; // Can't tell how many are in use, so don't add more

        left = Math.min(left, getFdLimit() / 2 - open.length);
        return Math.max(left, 0);
    }

    private static int getFdLimit()
    {
        if (fdLimit < 0)
        {
            fdLimit = 1024; // Conservative default if /proc can't be read
            try (BufferedReader r = new BufferedReader(new FileReader("/proc/self/limits")))
            {
                String line;
                while ((line = r.readLine()) != null)
                {
                    if (line.startsWith("Max open files"))
                    {
                        // "Max open files  <soft>  <hard>  files"
                        fdLimit = Integer.parseInt(line.substring("Max open files".length()).trim().split("\\s+")[0]);
                        break;
                    }
                }
            }
            catch (Exception e)
            {
                // Keep the default
            }
        }
        return fdLimit;
    }
}
