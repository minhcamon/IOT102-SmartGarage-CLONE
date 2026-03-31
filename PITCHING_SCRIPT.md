# KỊCH BẢN BẢO VỆ DỰ ÁN SMART GARAGE (Dành cho Sinh viên Kỹ thuật Phần mềm)

**Thời lượng mục tiêu:** 12 - 14 phút
**Mục tiêu cốt lõi:** Làm nổi bật tư duy *Software Engineering* (Kiến trúc phân tầng, State Machine, Non-blocking, Xử lý ngoại lệ) áp dụng lên *Phần cứng*, tối đa hóa điểm Block Diagram, Flowchart và Demo.

---

## PHẦN 1: MỞ ĐẦU & GIỚI THIỆU Ý TƯỞNG (1.5 - 2 phút)
*Mục tiêu: Chiếm thiện cảm, lấy trọn điểm Presentation (15đ) bằng phong thái tự tin và góc nhìn khác biệt.*

**Sinh viên trình bày (MC chính):**
> "Kính thưa Hội đồng Giám khảo, chúng em là nhóm [Tên nhóm]. Hôm nay chúng em rất vinh dự được trình bày dự án cuối kỳ: **Hệ thống Smart Garage**.
> 
> Là những sinh viên chuyên ngành Kỹ thuật Phần mềm, khi tiếp cận môn học IoT, chúng em không chỉ nhìn nhận Smart Garage như một mạch điện nối với cảm biến. Thay vào đó, chúng em thiết kế nó dưới góc độ một **Hệ thống phân tán (Distributed System)** thực thụ.
> 
> Trong đó, vi điều khiển ESP32 đóng vai trò là một thiết bị Biên (Edge Device) thu thập dữ liệu và phản xạ thời gian thực; kết hợp với giao thức MQTT đóng vai trò làm Message Broker mạnh mẽ; và bộ não điều khiển trung tâm được triển khai trên nền tảng Web App (bằng React/NodeJS). Mục tiêu của chúng em là tạo ra một hệ thống không chỉ "chạy được ở trạng thái lý tưởng", mà phải "chống chịu được lỗi (Fault-tolerant)" ở môi trường thực tế."

*(Chuyển slide sang Block Diagram)*
> "Sau đây, bạn [Tên thành viên 2] sẽ giải thích chi tiết kiến trúc của hệ thống thông qua Block Diagram."

---

## PHẦN 2: TRÌNH BÀY BLOCK DIAGRAM (2.5 - 3 phút)
*Mục tiêu: Đạt 15đ Block Diagram. Gây ấn tượng bằng thuật ngữ SE (Separation of Concerns, Client-Server, Pub/Sub).*

**Thành viên 2:**
> "Dạ trân trọng kính mời thầy cô nhìn lên sơ đồ Block Diagram.
> Để hệ thống dễ bảo trì và mở rộng, chúng em áp dụng nguyên lý Phân tách mối quan tâm (Separation of Concerns) và chia hệ thống thành 3 phân tầng (3-Tier Architecture) cực kỳ rõ rệt:
> 
> **1. Lớp Tương tác Vật lý (Perception / Edge Tier):** 
> Trái tim là ESP32 chịu trách nhiệm xử lý các luồng ngắt (interrupt) từ Cảm biến Siêu âm, Nhận dạng RFID và trạng thái Nút bấm cơ cơ học (Debounced Buttons). Nó trực tiếp điều khiển cơ cấu chấp hành là Động cơ Servo.
> 
> **2. Lớp Mạng & Truyền tải (Network Tier):** 
> Bỏ qua chuẩn HTTP chậm chạp, chúng em sử dụng Broker HiveMQ với giao thức MQTT chạy ẩn dưới lớp mã hóa SSL/TLS. Đây là kênh **Pub/Sub** giúp dữ liệu giữa phần cứng và phần mềm được luân chuyển 2 chiều với độ trễ tính bằng mili-giây, giải quyết hoàn toàn vấn đề Firewall hay NAT rườm rà.
> 
> **3. Lớp Ứng dụng & Dịch vụ (Application Tier):**
> Là giao diện Web App. Bất cứ khi nào trạng thái phần cứng thay đổi, Edge Device sẽ đẩy (Push) một chuỗi dữ liệu JSON chuẩn hóa lên MQTT. Ứng dụng Web lập tức bắt được sự kiện này (Event-driven) và render dữ liệu cảnh báo cho Host theo thời gian thực."

*(Gật đầu chuyển lời)*
> "Tuy nhiên, để các phần cứng tương tác mượt mà mà không tự gây xung đột, logic Code bên dưới vi điều khiển mới là phần quan trọng nhất. Xin mời bạn [Tên thành viên 3] trình bày Flowchart."

---

## PHẦN 3: TRÌNH BÀY FLOWCHART (3 - 3.5 phút)
*Mục tiêu: Đạt 15đ Flowchart. Nhấn mạnh vào thuật toán Non-blocking, State Machine (Cỗ máy trạng thái).*

**Thành viên 3:**
> "Cảm ơn bạn. Dạ thưa Hội đồng, điểm tự hào nhất của nhóm về mặt phần mềm nằm ở Flowchart này. 
> 
> Nỗi ám ảnh lớn nhất của hệ thống nhúng sinh viên là hiện tượng "Treo vòng lặp" vì hàm `delay()`. Do đó, chúng em đã đập bỏ hoàn toàn luồng code lập trình tuần tự (Sequential) và thiết kế lại theo hướng **Lập trình Đa nhiệm không block (Non-blocking Scheduler)**.
> 
> Xin thầy cô nhìn vào khối vòng lặp Chính (Main Loop):
> - **Thứ nhất**, logic điều khiển động cơ được trừu tượng hóa thành một **Cỗ máy trạng thái hữu hạn (Finite State Machine - FSM)** với 5 trạng thái: ĐÓNG, MỞ, ĐANG ĐÓNG, ĐANG MỞ và KHẨN CẤP. Hành vi của phần cứng bị khóa chặt trong các trạng thái này, sẽ không bao giờ có chuyện cửa vừa nhận lệnh kéo lên đã giật cục kéo xuống làm cháy motor.
> - **Thứ hai - Logic chống kẹt an toàn:** Ngay cả khi động cơ đang quay (Delay-less Sweep), luồng đọc Cảm biến Siêu âm vẫn liên tục quét với tần số 3 mili-giây. Sơ đồ nhánh này (chỉ tay vào nhánh Sonar) rẽ hướng tức thời nếu phát hiện vật cản: Nó sẽ can thiệp thẳng vào bộ nhớ máy trạng thái, ghi đè thành `EMERGENCY` và ép motor phanh gấp, nhả trả lại để cứu kẹt.
> 
> Mọi tiến trình từ đọc nút, gửi mạng, chạy motor đều được phân bổ Time-Slice cắt lớp thời gian độc lập. Một con chip lõi đơn nhưng tạo cảm giác chạy đa luồng rất trơn tru."

---

## PHẦN 4: DEMONSTRATION SA BÀN (4 - 5 phút)
*Mục tiêu: Đạt 20đ Demo. Phải thực hiện theo đúng 3 bước để tạo sự hồi hộp và chứng minh tính ổn định.*

**MC chính (Trở lại sa bàn):**
> "Trăm nghe không bằng một thấy. Tiếp theo, nhóm em xin phép được Demo thực tế độ trễ thấp và khả năng vận hành của hệ thống trên sa bàn."

**Bước 1: Giới thiệu vật lý (Dry-run)**
> "Đây là sa bàn của nhóm, gồm Module nhận dạng RFID, cửa Garage thu nhỏ chạy bằng Servo, cảm biến chống kẹt ở bên trong và giao diện Web App đang mở trực tiếp trên Laptop kết nối mạng 4G (để chứng minh không dùng chung mạng nội bộ)."

**Bước 2: Happy Path Demo (Luồng lý tưởng)**
> *(Vừa thao tác vừa thuyết minh)*
> "Kịch bản 1: Mở cửa bằng thẻ hợp lệ. Em đưa thẻ Master chạm vào mắt đọc... Máy báo tiếng bíp, cửa mở mượt mà. Ngay lập tức, thầy cô có thể thấy Web App trên màn hình Laptop cập nhật trạng thái "OPENED" chưa tới 0.5s."
> "Kịch bản 2: Vợ/Chồng chủ nhà mở cửa từ xa. Tại Web App em nhấn nút Đóng... Bản tin MQTT dội về, cửa lập tức tuân lệnh và hạ xuống, thay đổi màn hình OLED vật lý tại gara luôn."

**Bước 3: Edge Case Demo (Tính năng "Ăn Tiền" - Cứu kẹt / Xử lý ngoại lệ)**
> "Kịch bản 3: Xử lý sự cố (Safety Fallback). Sự khác biệt của tư duy phần mềm là lường trước rủi ro. Em sẽ hạ cửa xuống từ thẻ RFID, nhưng trong lúc cửa đang khép, em thò tay vào mô phỏng một em bé chạy ngang qua..."
> *(Thực hiện thò tay chặn cảm biến)*
> "Cửa lập tức phanh gấp, còi báo động kêu và Đảo chiều quay ngược lên trên để nhả vật cản. Đồng thời web app hiện cảnh báo EMERGENCY báo cho chủ nhà biết có sự cố kẹt cửa!"

---

## PHẦN 5: XỬ LÝ SỰ CỐ TRÊN SÂN KHẤU (RISK MITIGATION)
*Lưu ý: Không đọc phần này lúc thuyết trình, đây là BÍ KÍP ngầm cho nhóm xử lý nếu lôi xảy ra khi Demo.*

**Trường hợp 1: Chết Wi-Fi trường, Web không nhận, MQTT time-out.**
> *Bình tĩnh xử lý:* Không hoảng hốt bóp nút Reset cứng. Cứ trình diễn bằng Nút Bấm và Thẻ Từ vật lý trước. 
> *Câu nói cứu cánh:* "Dạ thưa hội đồng, do mạng Wi-Fi tại phòng bảo vệ đang bị nghẽn băng thông, hệ thống MQTT tạm thời timeout. TUY NHIÊN, đây chính là lúc ưu điểm lớn nhất của mô hình Edge Device phát huy: **Khả năng tự chủ Offline (Blind Fail-over)**. Thầy cô thấy đó, mất mạng nhưng hệ thống vật lý chống kẹt, quẹt thẻ và mở cửa vẫn hoạt động trơn tru bằng Data lưu trong bộ nhớ Flash mà không hề bị đơ sập (Crash). Nó sẽ tự Reconnect ẩn ở background khi quét thấy sóng."

**Trường hợp 2: Cảm biến siêu âm bị nhiễu nhảy loạn (Báo kẹt ảo).**
> *Bình tĩnh xử lý:* Nhanh tay lấy tay che lại bề mặt cảm biến hoặc dời bàn ra khỏi luồng sóng phản xạ của bức tường. Không đứng xúm lại quá đông che sóng. 
> *Câu nói cứu cánh:* "Cảm biến sóng âm hở trong môi trường phòng đông người thường bị phản xạ bởi quần áo. Trong thực tế tụi em sẽ dùng vỏ in 3D chống nhiễu hạt để cô lập phương tia bắn. Ngoài ra tụi em đang giới hạn cửa ngắm (Threshold) trong code để lọc bớt các thông số ngoại biên này ạ."

---

## PHẦN 6: KẾT LUẬN & MỜI Q&A (1 phút)
*Mục tiêu: Đẩy bóng sang chân Ban Giám khảo bằng một câu Chốt tự tin.*

**MC chính:**
> "Tóm lại, dự án Smart Garage của nhóm không đi tìm những phần cứng quá đắt tiền, mà biến phần cứng cơ bản trở nên thông minh và đáng tin cậy nhờ áp dụng triệt để các kỹ thuật **Software Engineering**: Non-blocking timer, State-machine, Hardware Debounce và Fault-Tolerance.
> 
> Chúng em đã sẵn sàng đón nhận những góc nhìn khắt khe và câu hỏi hóc búa từ Hội đồng Giám khảo để hoàn thiện bản thân hơn. Dạ, chúng em xin trân trọng kính mời Hội đồng đặt câu hỏi ạ!" 

*(Cả nhóm cúi chào đều và mỉm cười)*.
