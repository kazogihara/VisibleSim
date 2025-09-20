#include <climits>
#include <algorithm>
#include "catomsCombinationCode.h"
#include <fstream>
#include <iostream>
#include <stack>


void CatomsCombinationCode::worldRun() {
    // catom->setColor(GREEN);
    catom->getNeighborOnCell(catom->position + Cell3DPosition(-1, 0, 0)) != NULL;
}

void CatomsCombinationCode::startup() {
    if (getId() == 1) worldRun();
}

void CatomsCombinationCode::parseUserBlockElements(TiXmlElement *config) {
    const char *attr = config->Attribute("mobile");
    if (attr != nullptr) {
        string str(attr);
        if (str == "true" || str == "1" || str == "yes") {
            isMobile = true;
            std::cout << getId() << " is mobile!" << std::endl; // complete with your code
        }
    }
}