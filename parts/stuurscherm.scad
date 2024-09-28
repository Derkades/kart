include <BOSL2/std.scad>

stuur_inner     = 54;
lip_diepte      = 7;
kabel_d         = 7;
pcb_breedte     = 29;
pcb_hoogte      = 27.5;
scherm_breedte  = 26;
scherm_hoogte   = 15;
scherm_dikte    = 2;
schroefgat      = 2;
schroefgat_h    = 25;
schroefgat_v    = 24;

$fs = 1;
$fa = 1;

difference() {
    cyl(lip_diepte, d=stuur_inner);
    
    down(1.1)
    cyl(lip_diepte-scherm_dikte, d=stuur_inner-5);
    
//    back(15)
//    right(lip_diepte/2)
//    yrot(-45)
//    cyl(30, d=kabel_d);
    
    up(2)
    cuboid([scherm_breedte, scherm_hoogte, 5]);
    
//    for (x = [-12.5, 12.5]) {
//        for (y = [12, -12]) {
//            translate([x, y, 0])
//            cyl(10,d=2); 
//        }
//    }
}