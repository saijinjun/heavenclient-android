package com.saijinjun.heavenclient;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.HttpServer;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

import java.io.File;
import java.io.IOException;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Runs the downloader against a small local HTTP server that serves fake .nx files. */
public class AssetDownloaderTest {
    @Rule
    public TemporaryFolder folder = new TemporaryFolder();

    private HttpServer server;
    private final Map<String, byte[]> served = new HashMap<>();
    private final List<String> ranges = new ArrayList<>();
    private boolean honorRange = true;
    private File phone;

    @Before
    public void setUp() throws IOException {
        for (int i = 0; i < AssetDownloader.FILES.length; i++) {
            byte[] data = new byte[1000 + i * 37];
            for (int j = 0; j < data.length; j++) {
                data[j] = (byte) (i * 31 + j);
            }
            served.put(AssetDownloader.FILES[i], data);
        }

        server = HttpServer.create(new InetSocketAddress("127.0.0.1", 0), 0);
        server.createContext("/assets/", this::serve);
        server.start();
        phone = folder.newFolder("phone");
    }

    @After
    public void tearDown() {
        server.stop(0);
    }

    private String url() {
        return "http://127.0.0.1:" + server.getAddress().getPort() + "/assets/";
    }

    private void serve(HttpExchange exchange) throws IOException {
        String name = exchange.getRequestURI().getPath().substring("/assets/".length());
        byte[] data = served.get(name);
        if (data == null) {
            exchange.sendResponseHeaders(404, -1);
            exchange.close();
            return;
        }

        String range = exchange.getRequestHeaders().getFirst("Range");
        int start = 0;
        int code = 200;
        if (range != null) {
            ranges.add(name + " " + range);
            if (honorRange) {
                start = Integer.parseInt(range.replace("bytes=", "").replace("-", ""));
                code = 206;
            }
        }

        int length = data.length - start;
        boolean head = exchange.getRequestMethod().equals("HEAD");
        exchange.getResponseHeaders().set("Content-Length", Integer.toString(length));
        exchange.sendResponseHeaders(code, head ? -1 : length);
        if (!head) {
            try (OutputStream out = exchange.getResponseBody()) {
                out.write(data, start, length);
            }
        }
        exchange.close();
    }

    @Test
    public void downloadsEveryFileAndReportsProgress() throws IOException {
        AssetDownloader downloader = new AssetDownloader(url(), phone);
        assertEquals(AssetDownloader.FILES.length, downloader.missingFiles().size());

        Map<String, Long> sizes = downloader.remoteSizes();
        long total = 0;
        for (byte[] data : served.values()) {
            total += data.length;
        }
        assertEquals(total, downloader.bytesRemaining(sizes));

        long[] last = { 0 };
        downloader.download(sizes, (file, done, all) -> {
            assertTrue(done >= last[0]);
            assertEquals(all, sizes.values().stream().mapToLong(Long::longValue).sum());
            last[0] = done;
        });

        assertEquals(total, last[0]);
        assertTrue(downloader.missingFiles().isEmpty());
        for (String name : AssetDownloader.FILES) {
            assertArrayEquals(served.get(name), Files.readAllBytes(new File(phone, name).toPath()));
            assertFalse(new File(phone, name + ".part").exists());
        }
    }

    @Test
    public void resumesAPartialFileWithARangeRequest() throws IOException {
        byte[] map = served.get("Map.nx");
        Files.write(new File(phone, "Map.nx.part").toPath(), java.util.Arrays.copyOf(map, 400));

        AssetDownloader downloader = new AssetDownloader(url(), phone);
        Map<String, Long> sizes = downloader.remoteSizes();
        assertEquals(downloader.bytesRemaining(sizes),
                served.values().stream().mapToLong(d -> d.length).sum() - 400);

        downloader.download(sizes, (file, done, all) -> { });

        assertTrue(ranges.contains("Map.nx bytes=400-"));
        assertArrayEquals(map, Files.readAllBytes(new File(phone, "Map.nx").toPath()));
    }

    @Test
    public void startsOverWhenTheServerIgnoresRange() throws IOException {
        honorRange = false;
        byte[] npc = served.get("Npc.nx");
        Files.write(new File(phone, "Npc.nx.part").toPath(), java.util.Arrays.copyOf(npc, 300));

        AssetDownloader downloader = new AssetDownloader(url(), phone);
        downloader.download(downloader.remoteSizes(), (file, done, all) -> { });

        assertArrayEquals(npc, Files.readAllBytes(new File(phone, "Npc.nx").toPath()));
    }

    @Test
    public void skipsFilesAlreadyOnThePhone() throws IOException {
        Files.write(new File(phone, "UI.nx").toPath(), served.get("UI.nx"));

        AssetDownloader downloader = new AssetDownloader(url(), phone);
        Map<String, Long> sizes = downloader.remoteSizes();

        assertFalse(sizes.containsKey("UI.nx"));
        assertEquals(AssetDownloader.FILES.length - 1, sizes.size());
    }

    @Test
    public void reportsAMissingServerFile() throws IOException {
        served.remove("Morph.nx");

        try {
            new AssetDownloader(url(), phone).remoteSizes();
            fail("Expected an error for the missing Morph.nx");
        } catch (IOException e) {
            assertTrue(e.getMessage().contains("Morph.nx"));
        }
    }

    @Test
    public void reportsAnUnreachableServer() throws IOException {
        int port = server.getAddress().getPort();
        server.stop(0);

        try {
            new AssetDownloader("http://127.0.0.1:" + port + "/assets/", phone).remoteSizes();
            fail("Expected a connection error");
        } catch (IOException expected) {
            // The setup screen turns this into the "is Tailscale on?" message.
        }
    }
}
