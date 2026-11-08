package org.xonotic.android;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.RandomAccessFile;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.List;

/**
 * First-run launcher. Makes sure the Xonotic game data (PK3 files) is present in
 * the app's external files directory, downloading only the data PK3s out of the
 * official release ZIP via HTTP range requests (resumable), then starts the game.
 */
public class LauncherActivity extends Activity {
    /** Official release archive. Only Xonotic/data/*.pk3 entries are fetched. */
    static final String DATA_ZIP_URL = "https://dl.xonotic.org/xonotic-0.8.6.zip";
    static final String ENTRY_PREFIX = "Xonotic/data/";

    private TextView status;
    private ProgressBar progress;
    private Button downloadButton, playButton;
    private volatile boolean busy;
    private volatile long grandTotal;
    static final int PICK_ZIP = 42;
    private final Handler ui = new Handler(Looper.getMainLooper());

    static File baseDir(Activity a) {
        File ext = a.getExternalFilesDir(null);
        return ext != null ? ext : a.getFilesDir();
    }

    static File dataDir(Activity a) {
        File d = new File(baseDir(a), "data");
        d.mkdirs();
        return d;
    }

    static boolean hasGameData(Activity a) {
        File[] files = dataDir(a).listFiles();
        if (files == null) return false;
        for (File f : files) {
            String n = f.getName();
            if (n.startsWith("xonotic-") && n.endsWith("-data.pk3") && f.length() > 1000000) return true;
        }
        return false;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (hasGameData(this) && getIntent().getBooleanExtra("autostart", true)) {
            startGame();
            return;
        }
        buildUi();
    }

    private void buildUi() {
        int pad = (int) (24 * getResources().getDisplayMetrics().density);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setGravity(Gravity.CENTER);
        root.setPadding(pad, pad, pad, pad);
        root.setBackgroundColor(Color.rgb(12, 14, 22));

        TextView title = new TextView(this);
        title.setText("XONOTIC");
        title.setTextSize(40);
        title.setTextColor(Color.rgb(80, 200, 255));
        title.setGravity(Gravity.CENTER);
        root.addView(title);

        status = new TextView(this);
        status.setTextColor(Color.LTGRAY);
        status.setTextSize(16);
        status.setGravity(Gravity.CENTER);
        status.setPadding(0, pad / 2, 0, pad / 2);
        status.setText("Game data not found.\n\nDownload about 1.2 GB of official Xonotic 0.8.6 data, "
                + "or copy the .pk3 files from a desktop Xonotic install into:\n"
                + dataDir(this).getAbsolutePath());
        root.addView(status);

        progress = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        progress.setMax(1000);
        progress.setVisibility(View.GONE);
        root.addView(progress, new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT));

        downloadButton = new Button(this);
        downloadButton.setText("Download game data");
        downloadButton.setOnClickListener(v -> startDownload());
        root.addView(downloadButton);

        Button importButton = new Button(this);
        importButton.setText("Import Xonotic ZIP from device");
        importButton.setOnClickListener(v -> pickZip());
        root.addView(importButton);

        playButton = new Button(this);
        playButton.setText("Play");
        playButton.setEnabled(hasGameData(this));
        playButton.setOnClickListener(v -> startGame());
        root.addView(playButton);

        setContentView(root);
        downloadButton.requestFocus(); // D-pad / controller navigation
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        // Controller "A" acts like a tap on the focused button.
        if (keyCode == KeyEvent.KEYCODE_BUTTON_A) {
            View f = getCurrentFocus();
            if (f != null) { f.performClick(); return true; }
        }
        return super.onKeyDown(keyCode, event);
    }

    private void pickZip() {
        if (busy) return;
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType("*/*");
        i.putExtra(Intent.EXTRA_MIME_TYPES, new String[] { "application/zip", "application/x-zip-compressed", "application/octet-stream" });
        startActivityForResult(i, PICK_ZIP);
    }

    @Override
    protected void onActivityResult(int req, int res, Intent data) {
        super.onActivityResult(req, res, data);
        if (req != PICK_ZIP || res != RESULT_OK || data == null || data.getData() == null) return;
        final android.net.Uri uri = data.getData();
        long len = -1;
        try (android.content.res.AssetFileDescriptor fd = getContentResolver().openAssetFileDescriptor(uri, "r")) {
            if (fd != null) len = fd.getLength();
        } catch (Exception ignored) { }
        final long total = len;
        runJob(() -> {
            try (InputStream in = getContentResolver().openInputStream(uri)) {
                int n = ZipExtractor.extractDataPk3s(in, dataDir(this), total,
                        (name, done) -> setStatus("Extracting " + name + "\n" + (done >> 20) + " MB read", done, total));
                if (n == 0) throw new IOException("No .pk3 game files found in that ZIP");
            }
        });
    }

    private interface Job { void run() throws Exception; }

    private void runJob(Job job) {
        busy = true;
        downloadButton.setEnabled(false);
        progress.setVisibility(View.VISIBLE);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        new Thread(() -> {
            String err = null;
            try { job.run(); } catch (Exception e) { err = e.toString(); }
            final String error = err;
            ui.post(() -> {
                busy = false;
                getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                downloadButton.setEnabled(true);
                if (error == null && hasGameData(this)) {
                    status.setText("Game files ready.");
                    playButton.setEnabled(true);
                    playButton.requestFocus();
                } else {
                    status.setText("Failed: " + error + "\nTry again (downloads resume).");
                }
            });
        }, "xonotic-data-job").start();
    }

    private void startGame() {
        startActivity(new Intent(this, XonoticActivity.class));
        finish();
    }

    private void setStatus(final String s, final long done, final long total) {
        ui.post(() -> {
            status.setText(s);
            if (total > 0) progress.setProgress((int) (done * 1000 / total));
        });
    }

    private void startDownload() {
        if (busy) return;
        runJob(this::download);
    }

    // ------------------------------------------------------------------ ZIP range download

    static final class Entry {
        String name; int method; long csize, size, crc; long localHeaderOffset; long dataOffset;
    }

    private static HttpURLConnection open(String url, long from, long to) throws IOException {
        HttpURLConnection c = (HttpURLConnection) new URL(url).openConnection();
        c.setConnectTimeout(20000);
        c.setReadTimeout(60000);
        c.setRequestProperty("Range", "bytes=" + from + "-" + (to >= 0 ? Long.toString(to) : ""));
        c.connect();
        int code = c.getResponseCode();
        if (code != 206) throw new IOException("Server did not honor range request (HTTP " + code + ")");
        return c;
    }

    private static byte[] readRange(String url, long from, long to) throws IOException {
        HttpURLConnection c = open(url, from, to);
        try (InputStream in = c.getInputStream()) {
            int len = (int) (to - from + 1);
            byte[] buf = new byte[len];
            int off = 0, r;
            while (off < len && (r = in.read(buf, off, len - off)) > 0) off += r;
            if (off != len) throw new IOException("Short read");
            return buf;
        } finally {
            c.disconnect();
        }
    }

    private static long contentLength(String url) throws IOException {
        HttpURLConnection c = (HttpURLConnection) new URL(url).openConnection();
        c.setRequestMethod("HEAD");
        c.setConnectTimeout(20000);
        long len = c.getContentLengthLong();
        c.disconnect();
        if (len <= 0) throw new IOException("Unknown archive size");
        return len;
    }

    private List<Entry> listEntries(String url) throws IOException {
        long total = contentLength(url);
        int tailLen = (int) Math.min(total, 256 * 1024);
        byte[] tail = readRange(url, total - tailLen, total - 1);
        ByteBuffer b = ByteBuffer.wrap(tail).order(ByteOrder.LITTLE_ENDIAN);
        int eocd = -1;
        for (int i = tail.length - 22; i >= 0; i--)
            if (b.getInt(i) == 0x06054b50) { eocd = i; break; }
        if (eocd < 0) throw new IOException("ZIP directory not found");
        long cdSize = b.getInt(eocd + 12) & 0xffffffffL;
        long cdOffset = b.getInt(eocd + 16) & 0xffffffffL;
        byte[] cd = (cdOffset >= total - tailLen)
                ? java.util.Arrays.copyOfRange(tail, (int) (cdOffset - (total - tailLen)), (int) (cdOffset - (total - tailLen) + cdSize))
                : readRange(url, cdOffset, cdOffset + cdSize - 1);
        ByteBuffer d = ByteBuffer.wrap(cd).order(ByteOrder.LITTLE_ENDIAN);
        List<Entry> out = new ArrayList<>();
        int p = 0;
        while (p + 46 <= cd.length && d.getInt(p) == 0x02014b50) {
            int method = d.getShort(p + 10) & 0xffff;
            long crc = d.getInt(p + 16) & 0xffffffffL;
            long csize = d.getInt(p + 20) & 0xffffffffL;
            long usize = d.getInt(p + 24) & 0xffffffffL;
            int nlen = d.getShort(p + 28) & 0xffff, xlen = d.getShort(p + 30) & 0xffff, clen = d.getShort(p + 32) & 0xffff;
            long lho = d.getInt(p + 42) & 0xffffffffL;
            String name = new String(cd, p + 46, nlen, "UTF-8");
            if (name.startsWith(ENTRY_PREFIX) && name.endsWith(".pk3") && name.indexOf('/', ENTRY_PREFIX.length()) < 0) {
                if (method != 0 && method != 8) throw new IOException(name + ": unsupported ZIP method " + method);
                Entry e = new Entry();
                e.name = name.substring(ENTRY_PREFIX.length());
                e.method = method;
                e.csize = csize;
                e.size = usize;
                e.crc = crc;
                e.localHeaderOffset = lho;
                out.add(e);
            }
            p += 46 + nlen + xlen + clen;
        }
        if (out.isEmpty()) throw new IOException("No data PK3s in archive");
        return out;
    }

    private void download() throws IOException {
        setStatus("Reading archive index...", 0, 0);
        List<Entry> entries = listEntries(DATA_ZIP_URL);
        long grand = 0, grandDone = 0;
        for (Entry e : entries) grand += e.size;
        grandTotal = grand;
        File dir = dataDir(this);
        for (Entry e : entries) {
            File dst = new File(dir, e.name);
            if (dst.exists() && dst.length() == e.size) { grandDone += e.size; continue; }
            byte[] lh = readRange(DATA_ZIP_URL, e.localHeaderOffset, e.localHeaderOffset + 29);
            ByteBuffer h = ByteBuffer.wrap(lh).order(ByteOrder.LITTLE_ENDIAN);
            if (h.getInt(0) != 0x04034b50) throw new IOException("Bad local header for " + e.name);
            e.dataOffset = e.localHeaderOffset + 30 + (h.getShort(26) & 0xffff) + (h.getShort(28) & 0xffff);

            File part = new File(dir, e.name + ".part");
            // Stored entries resume byte-exact; deflated ones restart (inflater state can't resume).
            long have = (e.method == 0 && part.exists()) ? Math.min(part.length(), e.size) : 0;
            if (e.method != 0) part.delete();
            if (have < e.size) {
                HttpURLConnection c = open(DATA_ZIP_URL, e.dataOffset + have, e.dataOffset + e.csize - 1);
                final long base = grandDone;
                try (InputStream raw = c.getInputStream()) {
                    InputStream in = e.method == 8
                            ? new java.util.zip.InflaterInputStream(raw, new java.util.zip.Inflater(true), 1 << 16)
                            : raw;
                    ZipExtractor.extract(in, part, have, e.size, e.crc, have == 0,
                            (done) -> setStatus(String.format("Downloading + extracting %s\n%d / %d MB", e.name,
                                    (base + done) >> 20, grandTotal >> 20), base + done, grandTotal));
                } finally {
                    c.disconnect();
                }
            }
            if (part.length() != e.size) throw new IOException("Incomplete " + e.name);
            if (!part.renameTo(dst)) throw new IOException("Cannot finalize " + e.name);
            grandDone += e.size;
        }
        setStatus("Download complete.", grand, grand);
    }
}
