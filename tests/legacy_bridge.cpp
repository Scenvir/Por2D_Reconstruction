#include "legacy_bridge.hpp"

// Only valid portal transforms remain legacy invariants.
// The unsafe integer collision/placement behavior is deliberately not an oracle.
namespace legacy {
por2::Vec2 transform(por2::Vec2 value, const std::array<por2::Portal, 2>& portals, bool velocity) {
    // Independent transcription of the integer transform in the original source.
    std::array<int,2> x{},y{},face{},along{};
    for(int i=0;i<2;++i){
        const int d=portals[i].code-1;
        x[i]=(portals[i].tile.x+(1-(d&1))*(d&2)/2*3+(1-(d&2)/2)*(d&4)/4)*20;
        y[i]=(portals[i].tile.y+(2-(d&2))*(d&1)/2*3+(d&2)*(d&4)/8)*20;
        face[i]=((d>>1)-1)&3;along[i]=2*(d&1)+(d&2)/2;
    }
    const int vx=static_cast<int>(value.x),vy=static_cast<int>(value.y);
    int depth=0,tangent=0;
    switch(face[0]){case 0:depth=y[0]-vy;break;case 1:depth=vx-x[0];break;case 2:depth=vy-y[0];break;default:depth=x[0]-vx;}
    switch(along[0]){case 0:tangent=vy-y[0];break;case 1:tangent=x[0]-vx;break;case 2:tangent=y[0]-vy;break;default:tangent=vx-x[0];}
    if(velocity){
        switch(face[0]){case 0:depth=vy;break;case 1:depth=-vx;break;case 2:depth=-vy;break;default:depth=vx;}
        switch(along[0]){case 0:tangent=-vy;break;case 1:tangent=vx;break;case 2:tangent=vy;break;default:tangent=-vx;}
    }
    int ox=0,oy=0;
    switch(face[1]){case 0:oy=(velocity?-depth:y[1]+depth);break;case 1:ox=(velocity?depth:x[1]-depth);break;case 2:oy=(velocity?depth:y[1]-depth);break;default:ox=(velocity?-depth:x[1]+depth);}
    switch(along[1]){case 0:oy=(velocity?-tangent:y[1]+tangent);break;case 1:ox=(velocity?tangent:x[1]-tangent);break;case 2:oy=(velocity?tangent:y[1]-tangent);break;default:ox=(velocity?-tangent:x[1]+tangent);}
    return {static_cast<double>(ox),static_cast<double>(oy)};
}
CrossingLock crossingLock() {
    // Captured result of the original crossing-frame scenario. Keeping the fixture
    // here makes the published reconstructed repository self-contained.
    return {{{253,301},por2::Direction::Up},0};
}
}
