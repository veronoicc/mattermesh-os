#pragma once
#include "theme.h"
#include <string>
#include <vector>
namespace mm {
// Message Delivery history (DeliverySection): a header card (message preview and summary), a detail line (route, repeaters),
// the timeline (DeliveryRows two-line rows: time + marker, description; a count line under it) and the RESEND button.
constexpr int DeliveryRowH=34,DeliveryRows=4,DeliveryDetailY=Top+50,DeliveryListY=Top+66,DeliveryCountY=DeliveryListY+DeliveryRows*DeliveryRowH+4;
constexpr Rect DeliveryHeader={Margin,Top+4,W-2*Margin,44},DeliveryResend={Margin,Bottom-30,W-2*Margin,30};
inline Rect deliveryRowRect(int i){return {Margin,DeliveryListY+i*DeliveryRowH,W-2*Margin,DeliveryRowH-2};}
struct DeliveryView {
 struct Row {std::string when,marker,text;};
 bool found=false;// the message is in the journal
 std::string preview,summary,detail;std::vector<Row> rows;int offset=0;
};
}
