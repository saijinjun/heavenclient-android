package com.saijinjun.heavenclient;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * Downloads the game's .nx files from the server's web stack into the app's
 * data folder.
 *
 * Each file is written to name.part and renamed only once complete, so a
 * file without .part is always whole. An interrupted download resumes from
 * the .part file with an HTTP Range request. Plain Java with no Android
 * classes, so it can be tested on a desktop JVM.
 */
public final class AssetDownloader {
    /** Every file the client opens: Base plus the 15 in NxFiles.h, and UI_83 for the cash shop. */
    public static final String[] FILES = {
        "Base.nx", "Character.nx", "Effect.nx", "Etc.nx", "Item.nx", "Map.nx",
        "Mob.nx", "Morph.nx", "Npc.nx", "Quest.nx", "Reactor.nx", "Skill.nx",
        "Sound.nx", "String.nx", "TamingMob.nx", "UI.nx", "UI_83.nx"
    };

    private static final int CONNECT_TIMEOUT_MS = 8000;
    private static final int READ_TIMEOUT_MS = 30000;
    private static final int BUFFER_SIZE = 256 * 1024;

    /** Progress across all files. Called from the downloading thread. */
    public interface Listener {
        void onProgress(String file, long doneBytes, long totalBytes);
    }

    private final String baseUrl;
    private final File directory;
    private volatile boolean cancelled;

    /** baseUrl ends with a slash, e.g. http://<server>:8090/assets/ */
    public AssetDownloader(String baseUrl, File directory) {
        this.baseUrl = baseUrl.endsWith("/") ? baseUrl : baseUrl + "/";
        this.directory = directory;
    }

    public void cancel() {
        cancelled = true;
    }

    /** Files that are not fully downloaded yet. */
    public List<String> missingFiles() {
        List<String> missing = new ArrayList<>();
        for (String name : FILES) {
            if (!new File(directory, name).isFile()) {
                missing.add(name);
            }
        }
        return missing;
    }

    /**
     * Asks the server for the size of each missing file, in download order.
     * Throws when the server cannot be reached or lacks a file, which is the
     * first thing to fail when Tailscale is off.
     */
    public Map<String, Long> remoteSizes() throws IOException {
        Map<String, Long> sizes = new LinkedHashMap<>();
        for (String name : missingFiles()) {
            HttpURLConnection connection = open(name);
            try {
                connection.setRequestMethod("HEAD");
                int code = connection.getResponseCode();
                if (code != HttpURLConnection.HTTP_OK) {
                    throw new IOException("The server has no " + name + " (HTTP " + code + ")");
                }
                long size = connection.getContentLengthLong();
                if (size <= 0) {
                    throw new IOException("The server did not report the size of " + name);
                }
                sizes.put(name, size);
            } finally {
                connection.disconnect();
            }
        }
        return sizes;
    }

    /** Bytes still needed for the given files, counting what .part files already hold. */
    public long bytesRemaining(Map<String, Long> sizes) {
        long remaining = 0;
        for (Map.Entry<String, Long> entry : sizes.entrySet()) {
            remaining += Math.max(0, entry.getValue() - partFile(entry.getKey()).length());
        }
        return remaining;
    }

    /** Downloads every file in sizes, resuming partial ones. */
    public void download(Map<String, Long> sizes, Listener listener) throws IOException {
        long total = 0;
        long done = 0;
        for (Map.Entry<String, Long> entry : sizes.entrySet()) {
            total += entry.getValue();
            done += Math.min(entry.getValue(), partFile(entry.getKey()).length());
        }

        for (Map.Entry<String, Long> entry : sizes.entrySet()) {
            done = downloadFile(entry.getKey(), entry.getValue(), done, total, listener);
        }
    }

    private long downloadFile(String name, long size, long done, long total, Listener listener)
            throws IOException {
        File part = partFile(name);
        long have = part.length();

        // A .part larger than the server's copy is from an older file; start over.
        if (have > size) {
            done -= have;
            deleteOrThrow(part);
            have = 0;
        }

        if (have < size) {
            HttpURLConnection connection = open(name);
            try {
                if (have > 0) {
                    connection.setRequestProperty("Range", "bytes=" + have + "-");
                }

                int code = connection.getResponseCode();
                boolean append = code == HttpURLConnection.HTTP_PARTIAL;

                if (!append && code != HttpURLConnection.HTTP_OK) {
                    throw new IOException("Downloading " + name + " failed (HTTP " + code + ")");
                }

                // A server that ignores Range sends the whole file again.
                if (!append && have > 0) {
                    done -= have;
                    have = 0;
                }

                try (InputStream in = connection.getInputStream();
                     OutputStream out = new FileOutputStream(part, append)) {
                    byte[] buffer = new byte[BUFFER_SIZE];
                    int read;
                    while ((read = in.read(buffer)) != -1) {
                        if (cancelled) {
                            throw new IOException("Download cancelled");
                        }
                        out.write(buffer, 0, read);
                        have += read;
                        done += read;
                        listener.onProgress(name, done, total);
                    }
                }
            } finally {
                connection.disconnect();
            }
        }

        if (part.length() != size) {
            throw new IOException(name + " was cut short; try again to resume");
        }

        File finished = new File(directory, name);
        if (!part.renameTo(finished)) {
            throw new IOException("Could not save " + name);
        }

        // Pieces the game streamed earlier are superseded by the whole file.
        new File(directory, name + ".cache").delete();
        new File(directory, name + ".lazy").delete();

        return done;
    }

    private File partFile(String name) {
        return new File(directory, name + ".part");
    }

    private HttpURLConnection open(String name) throws IOException {
        HttpURLConnection connection = (HttpURLConnection) new URL(baseUrl + name).openConnection();
        connection.setConnectTimeout(CONNECT_TIMEOUT_MS);
        connection.setReadTimeout(READ_TIMEOUT_MS);
        // Byte counts must match the file on disk, so ask for no compression.
        connection.setRequestProperty("Accept-Encoding", "identity");
        return connection;
    }

    private static void deleteOrThrow(File file) throws IOException {
        if (file.exists() && !file.delete()) {
            throw new IOException("Could not delete " + file.getName());
        }
    }
}
