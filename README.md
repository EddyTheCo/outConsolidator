# Output Consolidator

The repo produce a console application that takes care of consolidating outputs created in certain address. 
It also has the ability to transfer the tokens to a more secure address.

## How to use It

```bash
./outConsolidator seed address_index_consolidation address_index_dust node_address receiving_address (node_jwt) 
```

example with JWT

```bash
./outConsolidator ef4593558d0c3ed9e3f7a2de766d33093cd72372c800fa47ab5765c43ca006b5 1 0 https://3216aae.online-server.cloud rms1qrzd2l6nh7s5ydnqwkzu8h7e5hgsyh7480mteyzajzd0dhjv2zhmwy9ksm6  eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJhdWQiOiIxMkQzS29vV1NSSFVaa3Fyc0hRN2FKbW9wWUhqa1RRZk5zaXJkeW5QWTZHdHRZaURuNEN1IiwianRpIjoiMTY4MjY3NzMwMCIsImlhdCI6MTY4MjY3NzMwMCwiaXNzIjoiMTJEM0tvb1dTUkhVWmtxcnNIUTdhSm1vcFlIamtUUWZOc2lyZHluUFk2R3R0WWlEbjRDdSIsIm5iZiI6MTY4MjY3NzMwMCwic3ViIjoiSE9STkVURVNUSEVSIn0.mKAmVL_eDDz-7yIpxnEai709iGz478lMRKWgPy5FS4s
```
example without JWT

```bash
./outConsolidator ef4593558d0c3ed9e3f7a2de766d33093cd72372c800fa47ab5765c43ca006b5 1 0 https://3216aae.online-server.cloud rms1qrzd2l6nh7s5ydnqwkzu8h7e5hgsyh7480mteyzajzd0dhjv2zhmwy9ksm6 
```
It is important that the address indexes are not equal.

## How it works

After executing the application the Dust and Consolidation address values are printed.
Every time a new output is created on the Dust Address this one will be consumed using the outputs on the Consolidation address.
So one needs to have some minimum deposit of funds on the Consolidation address to be able to consume the dust outputs on the Dust address.
When the total amount of the Consolidation outputs and the Dust output is greater or equal to 2 times the minimum storage deposit of an Output, an output with a minimum deposit will be created on the Consolidation address the rest of the tokens will be transfered to the receiving_address value.

