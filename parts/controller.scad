include <BOSL2/std.scad>

height = 100;
width = 170;
t = 0.8;
depth = 35;

difference() {
    union(){
        cuboid([width, height, t], anchor=BOTTOM);
        rect_tube(h=depth, size=[width+t*2, height+t*2], isize=[width, height], anchor=BOTTOM);

        up(depth-12.5)
        right(width/2)
        yrot(45)
        cuboid([20, height, t], anchor=TOP+RIGHT);
    }
    
    up(depth-12.5)
    right(width/2)
    yrot(45)
    cuboid([100, height+2*t, 20], anchor=BOTTOM);
    
    up(depth)
    cuboid([width, height, 10], anchor=BOTTOM);
}