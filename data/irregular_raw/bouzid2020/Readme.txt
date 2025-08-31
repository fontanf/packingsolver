From https://research.kent.ac.uk/clho/data-sets/#637-dataset

This folder contains two subfolders: "Dataset" and "Solutions"

------------------------------------------------------------------
I. The "Dataset" subfolder
------------------------------------------------------------------

It consists of 6 data files.

These data files are the test problems used in: 
M.C. Bouzid and S. Salhi (2020), An Investigation into the Packing of Rectangles into a Fixed Size Circular Container: Constructive and Metaheuristic Search Approaches, submitted to EJOR. 

The test problems are the files:
S100, S150, S200, R100, R150, R200. 

The format of the S data files is:
number of squares (n), 3 values for the container radius
for square i (i=1,2,...,n) in turn:
   length_of_square_side


The format of the R data files is:
number of rectangles (n), 3 values for the container radius
for rectangle i (i=1,2,...,n) in turn:
   rectangle_length rectangle_width

The dataset of Lopez and Besley (2018) is available at http://people.brunel.ac.uk/~mastjjb/jeb/orlib/files/

-------------------------------------------------------------------
II. The "Solutions" subfolder 
-------------------------------------------------------------------

It contains the solution files found in:

M.C. Bouzid and S. Salhi (2020), An Investigation into the Packing of Rectangles into a Fixed Size Circular Container: Constructive and Metaheuristic Search Approaches, submitted to EJOR. 

The 108 solutions files are separated into two subfolders, LB2018 and BS2020, which contain the (new) best solutions found for the instances of Lopez and Beasley (2018) and Bouzid and Salhi (2020) respectively.     

The name of a solution file follows the following syntax:

InstanceName_fraction_rotation_objective_value.txt

Where:
- InstanceName corresponds to the name of the instance

- fraction is 0.33, 0.5 or 0.66 which corresponds to the container's area fraction

- rotation is either r (rotation allowed) or nor (no rotation allowed)

- objective is either num (maximise the number of objects packed) or area (maximise the total area packed)

- value corresponds to the value of the solution described in the file

The solution file follows the following syntax: 

Solution value: XXXXXX

Elapsed time: XXXXXX secs 
---------------------------------

ID x                 y           
---------------------------------
i1 XXXXX XXXXX
i2 XXXXX XXXXX 
...
---------------------------------

Where the column:

- ID corresponds to the indices of the objects packed. 
A 'R' character can be added to the ID if the object is rotated by pi/2 in the solution.
 
- x and y correspond to the coordinates of the centre of the object packed. 
