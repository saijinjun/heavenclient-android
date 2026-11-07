package com.saijinjun.heavenclient;

import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.os.SystemClock;
import android.view.Gravity;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import java.io.File;
import java.util.Locale;
import java.util.Map;

/**
 * First screen of the app. When all game files are in the data folder, or
 * the player chose to stream them, it goes straight to the game. Otherwise
 * it offers to play right away, with the game fetching pieces of the files
 * as it needs them, or to download everything first.
 */
public class SetupActivity extends Activity {
    /** Refresh the progress text at most this often so the UI thread stays free. */
    private static final long PROGRESS_INTERVAL_MS = 250;
    private static final String PREFS = "setup";
    private static final String PREF_STREAM = "stream";
    /** The gold of the game's window rims. */
    private static final int GOLD = Color.rgb(232, 190, 92);

    private AssetDownloader downloader;
    private Map<String, Long> sizes;
    private Thread worker;

    private TextView status;
    private ProgressBar progress;
    private Button action;
    private Button secondary;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // The same folder the game reads from (SDL_AndroidGetExternalStoragePath).
        File directory = getExternalFilesDir(null);

        if (directory != null) {
            downloader = new AssetDownloader(BuildConfig.ASSETS_URL, directory);

            if (downloader.missingFiles().isEmpty() || prefs().getBoolean(PREF_STREAM, false)) {
                startGame();
                return;
            }
        }

        buildLayout();

        if (downloader == null) {
            showError("The phone's storage is not available right now. Close the app and try again.",
                    "Try again", v -> recreate());
            return;
        }

        checkServer();
    }

    @Override
    protected void onDestroy() {
        if (downloader != null) {
            downloader.cancel();
        }
        super.onDestroy();
    }

    private void buildLayout() {
        int pad = dp(24);

        TextView title = new TextView(this);
        title.setText(R.string.setup_title);
        title.setTextColor(GOLD);
        title.setTextSize(26);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        title.setShadowLayer(dp(2), 0, dp(1), Color.BLACK);
        title.setGravity(Gravity.CENTER);

        status = new TextView(this);
        status.setTextColor(Color.LTGRAY);
        status.setTextSize(16);
        status.setGravity(Gravity.CENTER);
        status.setPadding(0, dp(16), 0, dp(16));

        progress = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        progress.setMax(1000);
        progress.setVisibility(View.GONE);
        progress.setProgressTintList(ColorStateList.valueOf(GOLD));
        progress.setProgressBackgroundTintList(ColorStateList.valueOf(Color.rgb(40, 52, 88)));

        // The same navy and gold as the game's own windows, so the first
        // screen of the app already looks like the game
        action = styledButton(Color.rgb(236, 128, 28), Color.rgb(196, 88, 12));
        action.setVisibility(View.GONE);

        secondary = styledButton(Color.rgb(36, 48, 84), Color.rgb(22, 30, 58));
        secondary.setVisibility(View.GONE);

        // A card in the middle of a dark navy backdrop holds everything
        LinearLayout card = new LinearLayout(this);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setGravity(Gravity.CENTER);
        card.setPadding(pad, pad, pad, pad);
        GradientDrawable cardFill = new GradientDrawable(GradientDrawable.Orientation.TOP_BOTTOM,
                new int[] { Color.rgb(30, 40, 74), Color.rgb(14, 20, 42) });
        cardFill.setCornerRadius(dp(14));
        cardFill.setStroke(dp(2), GOLD);
        card.setBackground(cardFill);
        card.addView(title);
        card.addView(status);
        card.addView(progress, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT));

        LinearLayout.LayoutParams buttonSize = new LinearLayout.LayoutParams(
                dp(260), LinearLayout.LayoutParams.WRAP_CONTENT);
        buttonSize.topMargin = dp(12);
        card.addView(action, buttonSize);
        card.addView(secondary, new LinearLayout.LayoutParams(buttonSize));

        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setGravity(Gravity.CENTER);
        layout.setBackground(new GradientDrawable(GradientDrawable.Orientation.TOP_BOTTOM,
                new int[] { Color.rgb(12, 18, 40), Color.rgb(4, 6, 16) }));
        layout.setPadding(pad, pad, pad, pad);
        layout.addView(card, new LinearLayout.LayoutParams(
                Math.min(dp(520), getResources().getDisplayMetrics().widthPixels - 2 * pad),
                LinearLayout.LayoutParams.WRAP_CONTENT));

        setContentView(layout);
    }

    private void checkServer() {
        status.setText(R.string.setup_connecting);
        progress.setVisibility(View.GONE);
        action.setVisibility(View.GONE);
        secondary.setVisibility(View.GONE);

        runInBackground(() -> {
            try {
                Map<String, Long> found = downloader.remoteSizes();
                long remaining = downloader.bytesRemaining(found);
                long free = getExternalFilesDir(null).getUsableSpace();
                runOnUiThread(() -> offerDownload(found, remaining, free));
            } catch (Exception e) {
                runOnUiThread(() -> showError(getString(R.string.setup_unreachable,
                        BuildConfig.ASSETS_URL, e.getMessage()), "Try again", v -> checkServer()));
            }
        });
    }

    private void offerDownload(Map<String, Long> found, long remaining, long free) {
        sizes = found;

        status.setText(getString(R.string.setup_offer, gigabytes(remaining)));
        action.setText(R.string.setup_play);
        action.setOnClickListener(v -> {
            prefs().edit().putBoolean(PREF_STREAM, true).apply();
            startGame();
        });
        action.setVisibility(View.VISIBLE);

        secondary.setText(getString(R.string.setup_download, gigabytes(remaining)));
        secondary.setOnClickListener(v -> {
            // Leave some room so the phone is not filled to the last byte.
            if (remaining + 200L * 1024 * 1024 > free) {
                showError(getString(R.string.setup_no_space, gigabytes(remaining), gigabytes(free)),
                        "Check again", w -> checkServer());
            } else {
                download();
            }
        });
        secondary.setVisibility(View.VISIBLE);
    }

    private void download() {
        action.setVisibility(View.GONE);
        secondary.setVisibility(View.GONE);
        progress.setVisibility(View.VISIBLE);
        progress.setProgress(0);
        status.setText(R.string.setup_starting);

        // A multi-GB download takes a while; keep the screen from locking.
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        runInBackground(() -> {
            long started = SystemClock.elapsedRealtime();
            long[] last = { 0, 0 };

            try {
                long[] first = { -1 };

                downloader.download(sizes, (file, done, total) -> {
                    long now = SystemClock.elapsedRealtime();
                    if (first[0] < 0) {
                        first[0] = done;
                    }
                    if (now - last[0] < PROGRESS_INTERVAL_MS && done < total) {
                        return;
                    }
                    last[0] = now;

                    double seconds = Math.max(0.001, (now - started) / 1000.0);
                    double speed = (done - first[0]) / seconds;
                    int permille = total > 0 ? (int) (done * 1000 / total) : 0;
                    String text = getString(R.string.setup_progress, file,
                            gigabytes(done), gigabytes(total), megabytes((long) speed));

                    runOnUiThread(() -> {
                        progress.setProgress(permille);
                        status.setText(text);
                    });
                });

                runOnUiThread(this::startGame);
            } catch (Exception e) {
                runOnUiThread(() -> {
                    getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                    showError(getString(R.string.setup_failed, e.getMessage()), "Resume", v -> checkServer());
                });
            }
        });
    }

    private void showError(String message, String button, View.OnClickListener onClick) {
        if (isFinishing()) {
            return;
        }
        status.setText(message);
        progress.setVisibility(View.GONE);
        secondary.setVisibility(View.GONE);
        action.setText(button);
        action.setOnClickListener(onClick);
        action.setVisibility(View.VISIBLE);
    }

    private void startGame() {
        startActivity(new Intent(this, GameActivity.class));
        finish();
    }

    private SharedPreferences prefs() {
        return getSharedPreferences(PREFS, MODE_PRIVATE);
    }

    private void runInBackground(Runnable task) {
        worker = new Thread(task, "asset-download");
        worker.start();
    }

    /** A rounded, gold-rimmed button with a soft top-to-bottom shine. */
    private Button styledButton(int top, int bottom) {
        Button button = new Button(this);
        GradientDrawable fill = new GradientDrawable(GradientDrawable.Orientation.TOP_BOTTOM,
                new int[] { top, bottom });
        fill.setCornerRadius(dp(10));
        fill.setStroke(dp(2), GOLD);
        button.setBackground(fill);
        button.setTextColor(Color.WHITE);
        button.setTextSize(17);
        button.setTypeface(Typeface.DEFAULT_BOLD);
        button.setAllCaps(false);
        button.setShadowLayer(dp(1), 0, dp(1), Color.BLACK);
        button.setPadding(dp(16), dp(12), dp(16), dp(12));
        return button;
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

    private static String gigabytes(long bytes) {
        return String.format(Locale.US, "%.2f GB", bytes / (1024.0 * 1024 * 1024));
    }

    private static String megabytes(long bytes) {
        return String.format(Locale.US, "%.1f MB", bytes / (1024.0 * 1024));
    }
}
