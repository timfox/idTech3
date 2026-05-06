# AMPL (A Mathematical Programming Language)

**AMPL** is an algebraic modeling language for describing and solving large-scale optimization and scheduling problems. Syntax is close to conventional mathematical notation; models are typically separated from data (`.mod` / `.dat` / `.run`).

**Official site:** [https://www.ampl.com](https://www.ampl.com)

**Not part of this engine:** idTech3 does not embed or depend on AMPL. This page is a **standalone reference** only.

## Origins and license

Designed by Robert Fourer, David M. Gay, and Brian W. Kernighan at Bell Labs; first appeared in 1985. The **translator** is proprietary (AMPL Optimization LLC); the **AMPL Solver Library (ASL)** for reading `.nl` files and automatic differentiation is open source. **AMPL/MP** is an open-source library for building certain solver classes.

## Problem classes (examples)

Linear and mixed-integer programming, quadratic and mixed-integer QP, nonlinear and MINLP, second-order cone programming, complementarity (MPECs), constraint programming, global optimization, and others depending on the linked solver.

## Solver interaction

AMPL normally invokes a solver in a **separate process** via a well-defined **`.nl`** interface, which isolates solver crashes from the interpreter and allows mixed 32/64-bit translator/solver combinations.

## Sample model (Dantzig transportation)

Classic linear program: minimize freight cost subject to plant capacity and market demand.

```ampl
set Plants;
set Markets;

param Capacity{p in Plants};
param Demand{m in Markets};
param Distance{Plants, Markets};
param Freight;

param TransportCost{p in Plants, m in Markets} :=
    Freight * Distance[p, m] / 1000;

var shipment{Plants, Markets} >= 0;

minimize cost:
    sum{p in Plants, m in Markets} TransportCost[p, m] * shipment[p, m];

s.t. supply{p in Plants}:
    sum{m in Markets} shipment[p, m] <= Capacity[p];

s.t. demand{m in Markets}:
    sum{p in Plants} shipment[p, m] >= Demand[m];

data;

set Plants := seattle san-diego;
set Markets := new-york chicago topeka;

param Capacity :=
    seattle   350
    san-diego 600;

param Demand :=
    new-york 325
    chicago  300
    topeka   275;

param Distance : new-york chicago topeka :=
    seattle        2.5      1.7     1.8
    san-diego      2.5      1.8     1.4;

param Freight := 90;
```

## Solvers (partial)

AMPL supports many solvers, including open-source and commercial examples: **CBC**, **CLP**, **IPOPT**, **SCIP**, **HiGHS**, **CPLEX**, **Gurobi**, **MOSEK**, **KNITRO**, **SNOPT**, **MINOS**, **Bonmin**, **Couenne**, **Gecode**, **JaCoP**, and others. See the current list at [Solvers – AMPL](https://ampl.com/products/solvers/).

## APIs and tooling

Official APIs exist for **Python**, **R**, **C++**, **C#**, **MATLAB**, and **Java**. NEOS and similar services accept AMPL models remotely.

## See also

- [JuMP](https://jump.dev/) (Julia)
- [Pyomo](http://www.pyomo.org/) (Python)
- [GNU MathProg](https://www.gnu.org/software/glpk/) (GLPK subset of AMPL)
- [NEOS Server](https://neos-server.org/)

## References

- Fourer, Gay, Kernighan, *AMPL: A Modeling Language for Mathematical Programming* (2003), Duxbury/Brooks-Cole. ISBN 978-0-534-38809-6. [Online book](https://ampl.com/resources/the-ampl-book/)
- Fourer, Gay, Kernighan, “A Modeling Language for Mathematical Programming,” *Management Science* 36(5), 1990. [DOI 10.1287/mnsc.36.5.519](https://doi.org/10.1287/mnsc.36.5.519)

---

*Summary incorporates material from the Wikipedia article [“AMPL”](https://en.wikipedia.org/wiki/AMPL) (retrieved 2026); [Creative Commons Attribution-ShareAlike 4.0 International License](https://creativecommons.org/licenses/by-sa/4.0/).*
