#ifndef simpleColorCode_H_
#define simpleColorCode_H_
#include "robots/okteen/okteenBlockCode.h"

static const int BROADCAST_MSG=1001;

using namespace Okteen;

class SimpleColorCode : public OkteenBlockCode {
private:
    int distance;
public :
    SimpleColorCode(OkteenBlock *host):OkteenBlockCode(host) {};
    ~SimpleColorCode() {};

    void startup() override;
    void myBroadcastFunc(const std::shared_ptr<Message> msg,P2PNetworkInterface *sender);

/*****************************************************************************/
/** needed to associate code to module                                      **/
    static BlockCode *buildNewBlockCode(BuildingBlock *host) {
        return (new SimpleColorCode((OkteenBlock*)host));
    };
/*****************************************************************************/
};
#endif /* simpleColorCode_H_ */
