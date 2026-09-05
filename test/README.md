# Running the tests

No IDE, no framework, no platform. One compiler invocation.

```
g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -o tests test_geogrid/test_geogrid.cpp ../src/services/GeoGrid.cpp
./tests
```

One case in isolation, which is how the density tooling works:

```
./tests zone_arctic_west_of_prime_meridian_is_not_svalbard
```

## Coverage and density

```
g++ -std=c++17 -O0 -g --coverage -I. -I../src -c ../src/services/GeoGrid.cpp -o GeoGrid.o
g++ -std=c++17 -O0 -g --coverage -I. -I../src -c test_geogrid/test_geogrid.cpp -o tests.o
g++ --coverage GeoGrid.o tests.o -o test_cov
python3 density.py
```

`density.py` runs every case in its own process, keeps a separate gcov profile
for each, and reports how many distinct cases touched each line.

The deployable firmware build never sees any of this. Nothing in
`platformio.ini` references `test/`, and the test binary is built by a
standalone g++ call that knows nothing about the device.
