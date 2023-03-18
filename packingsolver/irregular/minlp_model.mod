# Heigh of the container.
param hi;

# Number of circles.
param nc;
# Set of circles.
set CIRCLES = 1..nc;
# Radius of the circles.
param rc {CIRCLES};
# x-coordinates of the center of the circles.
var XC {CIRCLES};
# y-coordinates of the center of the circles.
var YC {CIRCLES};

var X_Max;

minimize Objective: X_Max;

subject to Link_X_Max_XC {j in CIRCLES}:
    X_Max >= XC[j] + rc[j];

# Each pair of circle must not intersect.
subject to circle_circle_intersections {j1 in CIRCLES, j2 in CIRCLES: j1 < j2}:
    (XC[j2] - XC[j1])^2 + (YC[j2] - YC[j1])^2 >= (rc[j1] + rc[j2])^2;

# Each circle must be inside the container.
subject to circle_container_intersections_bottom {j in CIRCLES}:
    YC[j] - rc[j] >= 0;
subject to circle_container_intersections_top {j in CIRCLES}:
    YC[j] + rc[j] <= hi;
subject to circle_container_intersections_left {j in CIRCLES}:
    XC[j] - rc[j] >= 0;
