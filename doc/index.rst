PackingSolver's documentation
=============================

.. toctree::
   :maxdepth: 2
   :caption: Contents:

`PackingSolver` is a software package dedicated to the practical resolution of cutting and packing problems.

`PackingSolver` takes as input:
* A set of pieces to cut/pack called **items**.
* A set of containers from which to cut / in which to pack these items, called **bins**.
* A set of parameters for the optimization

Then `PackingSolver` outputs the cutting/loading plans.

PackingSolver solves multiple problem types:

| Problem types            |  Examples |
:------------------------- |:-------------------------
[`rectangleguillotine`](rectangleguillotine)<ul><li>Items: two-dimensional rectangles</li><li>Only edge-to-edge cuts are allowed</li></ul>  |  <img src="https://github.com/fontanf/packingsolver/blob/master/img/rectangleguillotine.png" align=center width="512">
[`rectangle`](rectangle)<ul><li>Items: two-dimensional rectangles</li></ul>  |  <img src="https://github.com/fontanf/packingsolver/blob/master/img/rectangle.png" align=center width="512">
[`boxstacks`](boxstacks)<ul><li>Items: three-dimensional rectangular parallelepipeds</li><li>Items can be stacked; a stack contains items with the same width and length</li></ul>  |  <img src="https://github.com/fontanf/packingsolver/blob/master/img/boxstacks.png" align=center width="512">
[`onedimensional`](onedimensional)<ul><li>Items: one-dimensional items</li></ul>  |  <img src="https://github.com/fontanf/packingsolver/blob/master/img/onedimensional.png" align=center width="512">
[`irregular`](irregular)<ul><li>Items: two-dimensional polygons</li></ul>  |  <img src="https://github.com/fontanf/packingsolver/blob/master/img/irregular.png" align=center width="512">
