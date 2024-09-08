package com.example.mte_demo;

import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import com.example.mte_demo.databinding.ActivityMainBinding;

public class MainActivity extends AppCompatActivity {

    // Used to load the 'mte_demo' library on application startup.
    static {
        System.loadLibrary("mte_demo");
    }

    private ActivityMainBinding binding;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        Button allocate = binding.action1;
        allocate.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v)  {
                double[] allocationTime = new double[257];
                for(int i = 0;i<257;i++){
                    allocationTime[i]= allocateMemory(i*4096);
                }
                for(int i = 0;i<257;i++){
                    Log.v("Time", "Time for " + 4096*i*16 + " elements: " + allocationTime[i]);
                }
            }
        });

        Button fill = binding.action2;
        fill.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v)  {
                double[] allocationTime = new double[257];
                for(int i = 0;i<257;i++){
                    allocationTime[i]= fillBuffer(i*4096);
                }
                for(int i = 0;i<257;i++){
                    Log.v("Time", "Time filling " + 4096*i*16 + " elements: " + allocationTime[i]);
                }
            }
        });

        Button fillRandom = binding.fillRandom;
        fillRandom.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                new Thread(new Runnable() {
                    @Override
                    public void run() {
                        double[] allocationTime = new double[129];
                        for(int i = 0; i < 129; i++) {
                            allocationTime[i] = fillBufferRandom(i * 2);
                        }
                        for(int i = 0; i < 129; i++) {
                            // Use runOnUiThread to post results back to the main thread
                            final int index = i;
                            runOnUiThread(new Runnable() {
                                @Override
                                public void run() {
                                    Log.v("Time", "Time filling random " + 2 * index * 16 + " elements: " + allocationTime[index]);
                                }
                            });
                        }
                    }
                }).start();
            }
        });

        Button traverse = binding.action3;
        traverse.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v)  {
                double[] allocationTime = new double[257];
                for(int i = 0;i<257;i++){
                    allocationTime[i]= traverseBuffer(4096*i);
                }
                for(int i = 0;i<257;i++){
                    Log.v("Time", "Time traversing " + 4096*i*16 + " elements: " + allocationTime[i]);
                }
            }
        });

        Button traverseRandom = binding.traverseRandom;
        traverseRandom.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                new Thread(new Runnable() {
                    @Override
                    public void run() {
                        double[] allocationTime = new double[129];
                        for(int i = 0; i < 129; i++) {
                            allocationTime[i] = traverseBufferRandom(i * 2048);
                        }
                        for(int i = 0; i < 129; i++) {
                            // Use runOnUiThread to post results back to the main thread
                            final int index = i;
                            runOnUiThread(new Runnable() {
                                @Override
                                public void run() {
                                    Log.v("Time", "Time reading random " + 2048 * index * 16 + " elements: " + allocationTime[index]);
                                }
                            });
                        }
                    }
                }).start();
            }
        });

        Button mismatch = binding.tagMismatch;
        mismatch.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v)  {
                double[] allocationTime = new double[129];
                for(int i = 0;i<129;i++){
                    allocationTime[i]= mismatch(i*2);
                }
                for(int i = 0;i<129;i++){
                    Log.v("Time", "Time crashing " + 2*i*16 + " elements: " + allocationTime[i]);
                }
            }
        });

        // Example of a call to a native method
        TextView tv = binding.sampleText;
        tv.setText(hasMTE());
    }

    /**
     * A native method that is implemented by the 'mte_demo' native library,
     * which is packaged with this application.
     */
    public native String stringFromJNI();
    public native String hasMTE();

    public native void UserAfterFree();

    public native double allocateMemory(long  elements);
    public native double traverseBuffer(long elements);
    public native double traverseBufferRandom(long elements);
    public native double fillBuffer(long elements);
    public native double fillBufferRandom(long elements);
    public native double mismatch(long elements);
}