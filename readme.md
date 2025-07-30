# Flexpoch

Code of the publication: _"Flexpoch: Feature-rich 64-bit DateTime Encoding"_

To cite our work, please use the following BibTex code:

```
@inproceedings{fdl:2025,
  year         = { 2025 },
  location     = { St. Goar, Germany },
  booktitle    = { 2025 Forum on Specification & Design Languages (FDL) },
  author       = { Emanuel Regnath and Andreas Finkenzeller and Sebastian Steinhorst },
  title        = { Flexpoch: Feature-rich 64-bit DateTime Encoding },
  pages        = { 1-5 },
  doi          = {  },
  month        = {  },
}
```

---



## Build

1. Switch to the repository's root directory

2. Compile source code:
    ```
    make all
    ```


## Run

Run flexpoch (valid formats FMT=[iso|unix|fp]):
```
./bin/fp --from-<FMT> VALUE --to-<FMT>
```

Examples:
```
./bin/fp --from-fp 0x00680FAA131FA3C5 --to-iso                  # Output: 2025-04-28T18:17:23.123+02:00
./bin/fp --from-fp 0x00680FAA131FA3C5 --to-unix                 # Output: 1745857043

./bin/fp --from-unix 1745857043 --to-fp                         # Output: 0x00680FAA13800007
./bin/fp --from-iso 2025-04-28T18:17:23.123+02:00 --to-fp       # Output: 0x00680FAA131F63C5
```


## Test

### Performance

To run performance tests on your machine, execute the test binary:
```
./bin/test_all
```

Note: In case you run on ARM chipsets (e.g., Raspberry Pi), you need to compile and insert an additional kernel module to enable user access to the required registers before you can run the tests:
```
cd kernel_mod
make
insmod enable_ccr.ko
```

### Unit Tests

We have a set of unit tests to verify the correct protocol implementation. To run them, execute the test script:
```
bash ./tests/run_tests.sh
```
