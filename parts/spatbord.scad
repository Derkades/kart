include <BOSL2/std.scad>
include <BOSL2/screws.scad>

wheel_r = 193/2;
wheel_h = 50;
fender_ir = wheel_r + 8;
fender_or = fender_ir + 1.6;
shaft_r = 60 / 2;
fender_h = wheel_h + 10;
fender_cover = 0.6;
cover_t = 2;
screw_distance = 50;
screw_spec = "M4";
screw_r = 8;
screw_h = 6;

// set to 1 for final render, or higher to speed up render
$fa = 1;
$fs = 1;

module fender() {    
    difference() {
        union() {            
            tube(ir=fender_ir, or=fender_or, h=fender_h);
            
            // cover
            down(fender_h/2)
            cyl(r=fender_or, h=cover_t, anchor=TOP);
           
            // screw holes
            for (x = [-screw_distance, screw_distance]) {
                translate([x, screw_distance, -fender_h/2-cover_t])
                cyl(r=screw_r, h=screw_h, anchor=BOTTOM);
            }
        }
        
        // remove bottom half
        fwd(fender_or)
        down(cover_t)
        cuboid([fender_or*2, fender_or*2*(1-fender_cover), fender_h+cover_t*2+0.01], anchor=FRONT);
        
        // cutout for shaft
        down(fender_h/2+cover_t/2)
        cyl(r=shaft_r, h=cover_t+0.01)
        cuboid([shaft_r*2, wheel_r, cover_t+0.01], anchor=BACK);
        
        // screw holes
        for (x = [-screw_distance, screw_distance]) {
            translate([x, screw_distance, -fender_h/2]) {
                nut_trap_inline(spec=screw_spec, length=screw_h-cover_t+0.01, anchor=BOTTOM);
                screw_hole(spec=screw_spec, length=cover_t+0.01, anchor=TOP);
            }
 
        }
    }
} 

//#cyl(r=wheel_r, h=wheel_h);
fender();