<div class="build-instructions">
    <div class="platform windows">
        <h3>Windows</h3>
        <div class="code-block">
            <pre><code>git clone https://github.com/timfox/idtech3
cd idtech3
mkdir build && cd build
cmake ..
cmake --build .</code></pre>
        </div>
    </div>

    <div class="platform linux">
        <h3>Linux</h3>
        <div class="code-block">
            <pre><code>git clone https://github.com/timfox/idtech3
cd idtech3
mkdir build && cd build
cmake ..
make -j$(nproc)</code></pre>
        </div>
    </div>

    <div class="platform macos">
        <h3>macOS</h3>
        <div class="code-block">
            <pre><code>git clone https://github.com/timfox/idtech3
cd idtech3
mkdir build && cd build
cmake ..
make -j$(sysctl -n hw.ncpu)</code></pre>
        </div>
    </div>
</div> 