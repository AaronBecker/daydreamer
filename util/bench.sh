# Meant to be invoked from the root directory.
# A bit hacky, but whatever.
for i in `seq 1 10`; do
    ./target/release/daydreamer util/bench.rc > /dev/null
done
