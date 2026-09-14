# Local benchmark results

Measured on 2026-09-14 with a Release build, x86_64, 16.2.1 (GCC).

CPU: 12th Gen Intel(R) Core(TM) i5-1235U. One measured run per configuration; no warm-up, no confidence intervals. Shared host load and frequency scaling may affect results.

Command: `./build/lru_benchmark 200000`. Each implementation receives the identical seeded trace and starts empty.

| Workload | Capacity | Hash/list ns/request | Vector ns/request | Vector ÷ hash/list | Hits |
|---|---:|---:|---:|---:|---:|
| uniform | 64 | 38.7176 | 32.8257 | 0.847825× | 49890 |
| hot90 | 64 | 10.1484 | 28.0699 | 2.76594× | 185061 |
| uniform | 512 | 39.6039 | 213.192 | 5.38311× | 49946 |
| hot90 | 512 | 14.5926 | 141.431 | 9.69201× | 184842 |
| uniform | 4096 | 41.7889 | 1546.45 | 37.0062× | 49880 |
| hot90 | 4096 | 16.7462 | 964.434 | 57.5911× | 183620 |

Both implementations produced matching checksums and hit counts for all six traces. These results illustrate scaling under these particular workloads; they do not establish a universal speedup. Raw data: [benchmark-results.csv](benchmark-results.csv).
