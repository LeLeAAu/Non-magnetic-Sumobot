# English
# SumoBot V1 - UTC Arena Project

A fully autonomous sumo robot project designed for competition, featuring a maximum footprint of 15x15cm and a weight limit of 1.5kg.

## Combat Record

// This documentation was compiled long after the competition, and due to a lack of photo-taking habits at the time, no actual photos or videos are available.
// Only this meme file remains:

https://github.com/user-attachments/assets/25ba5a70-891f-4cdf-a935-8837fe36bc16

## Context & Competition Rules

This project was developed based on a specific set of competition rules (refined after a year of field experience). The primary mechanical and programming challenges included:

* **"Slippery" Arena:** A circular ring with a 100cm diameter and a 5cm white border. Notably, the floor is non-metallic, making it impossible to use magnets to increase downforce/friction.
* **3-Second Rule:** The bot must execute a 3-second delay program before initiating movement.
* **Combat Goal:** Victory is achieved by pushing the opponent out of the ring or flipping them over.
* **Weight Limit:** Maximum mass (m_max) = 1.5kg.

## Hardware Specifications

The bot's performance stems from a low center of gravity concentrated toward the front scoop, combined with the following components:

* **Microcontroller:** Arduino Uno R3.
* **Motors:** 2x JGY370 12V 210RPM DC Worm Gear Motors, with a rated torque of 1.7kg.cm.
* **Drivetrain:** 2x V2 65mm Wheels.
* **Motor Drivers:** 2x BTS7960.
* **Power System:** 3-cell 18650 Li-ion battery pack (3S configuration, ~11.1V) providing direct current to the motors. An LM2596 Buck converter is used to step down the voltage (12V to 5V) for the Arduino and sensors. *Note: The system runs directly without a BMS (Battery Management System) protection circuit.*
* **Chassis:** Aluminum front scoop; the remaining frame is constructed from 2mm thick PVC.

## Sensor System & "Last-Minute Failures"

The original design utilized **4x RCWL-1601 ultrasonic sensors** (for opponent detection) and **4x TCRT5000 infrared (IR) sensors** (for white border detection).

However, **harsh combat conditions** led to the complete failure of the IR edge sensors during the actual competition. Consequently, the official source code used was a *Fallback* version relying entirely on ultrasonic data (File: "Nếu cảm biến IR biên không hoạt động - 4IR + 4US.ino").

## Software Strategy

Due to the lack of border detection ("blindness"), the bot was forced into an aggressive offensive and continuous searching mode. The source code utilizes the "NewPing" library and is built on a State Machine architecture:

1. **Search States (SEARCH_LOCAL_SCAN, SEARCH_CRUISE):** The robot performs on-the-spot scans or moves in small circles (moveSmallCircleLeft, moveSmallCircleRight) to expand its field of view.
2. **Attack States:**
    * **ATTACK_CORNER:** Triggered when the opponent is detected within the narrow range of both front sensors, initiating cornering maneuvers.
    * **ATTACK_FLANK:** Executes a flanking curve attack when the opponent is at mid-range (30-45cm).
    * **ATTACK_RETREAT_TURN:** A specialized tactic to quickly reverse and turn 45 degrees immediately after a PUSH phase, preventing the bot from accidentally driving itself out of the ring due to loss of border orientation.

## Future Improvements for V2

* Transition to a professional 3D CAD design workflow (using Fusion 360 or Tinkercad) rather than manual PVC cutting.
* Upgrade the microcontroller: The Arduino Uno R3's 32KB Flash and 2KB RAM often create bottlenecks when handling multiple sensor interrupts. Migrating to an STM32 platform would offer a significant leap in multi-threaded processing speeds.
* Re-equip with higher-quality optical/IR sensors to optimize edge-tracking capabilities.

## Notes

* When reusing the code, ensure to recalibrate parameters and debug constant values.
* The "No-IR" file was modified during active combat, so its logic may be slightly more refined than the original version.

---
*This project is distributed under the MIT License.*

# Vietnamese
# SumoBot V1 - Dự án Đấu trường UTC

Một dự án robot tự động hoàn toàn tham gia thi đấu Sumo, được thiết kế với kích thước tối đa 15x15cm và trọng lượng dưới 1.5kg. 

## Thực chiến

// Do viết file này sau khi thực chiễn đã lâu và không có thói quen chụp ảnh -> không có ảnh và video thực tế.
// Còn giữ đúng cái file meme này


https://github.com/user-attachments/assets/25ba5a70-891f-4cdf-a935-8837fe36bc16


## Bối cảnh & Luật thi đấu

Dự án này được thiết kế dựa trên bộ luật thi đấu đặc thù (được tổng hợp lại sau 1 năm thực chiến). Những thách thức lớn nhất về mặt cơ khí và lập trình bao gồm:
* **Sàn đấu "trơn trượt":** Sàn tròn đường kính 100cm, có viền trắng 5cm. Đặc biệt, sàn không có kim loại, do đó không thể sử dụng nam châm để tăng ma sát ép mặt đường.
* **Luật 3 giây:** Bot phải có đoạn chương trình chờ 3 giây trước khi bắt đầu di chuyển.
* **Giao chiến:** Đẩy đối thủ ra ngoài hoặc làm đối thủ lật ngã là thắng.
* **Giới hạn khối lượng":** m_max = 1.5kg.

## Cấu hình Phần cứng (Hardware Specs)

Sức mạnh của bot đến từ thiết kế trọng tâm thấp dồn về phần ủi phía trước dưới, kết hợp cùng các linh kiện:

* **Vi điều khiển:** Arduino Uno R3.
* **Động cơ:** 2x DC Worm Gear JGY370 12V 210RPM, moment xoắn tải 1.7kg.cm.
* **Truyền động:** 2 Bánh xe V2 65mm.
* **Driver Động cơ:** 2x BTS7960.
* **Năng lượng:** 3 cell pin 18650 tạo nguồn 3S (~11.1V) cấp dòng trực tiếp cho động cơ. Sử dụng module giảm áp LM2596 Buck (hạ từ 12V xuống 5V) để nuôi Arduino và cảm biến. *Lưu ý: Hệ thống chạy trực tiếp không qua mạch bảo vệ BMS*.
* **Khung vỏ:** Phần ủi bằng nhôm, phần khung còn lại là nhựa PVC dày 2mm.

## Hệ thống Cảm biến & "Sự cố phút chót"

Kế hoạch ban đầu là sử dụng **4 cảm biến siêu âm RCWL-1601** (để dò đối thủ) và **4 cảm biến hồng ngoại tcrt5000** (để bám biên trắng). 

Tuy nhiên, **thực tế chiến trường khắc nghiệt**: Các cảm biến IR biên đã hoàn toàn chết khi thi đấu thực tế. Do đó, mã nguồn chính thức được sử dụng là một bản *Fallback* hoàn toàn dựa vào sóng siêu âm (File: `Nếu cảm biến IR biên không hoạt động - 4IR + 4US.ino`).

## Chiến thuật & Lập trình (Software Strategy)

Do bị "mù" ranh giới sàn đấu, bot buộc phải chơi theo thiên hướng tấn công chủ động và dò tìm liên tục. Mã nguồn sử dụng thư viện `NewPing`  và được xây dựng theo kiến trúc State Machine (Máy trạng thái):

1. **Trạng thái Tìm kiếm (`SEARCH_LOCAL_SCAN`, `SEARCH_CRUISE`):** * Robot xoay quét tại chỗ hoặc di chuyển theo các vòng tròn nhỏ (`moveSmallCircleLeft`, `moveSmallCircleRight`) để mở rộng góc nhìn.
2. **Trạng thái Tấn công (Attack States):**
   * **`ATTACK_CORNER`:** Kích hoạt khi đối thủ nằm trong tầm ngắm hẹp của cả 2 cảm biến trước, thực hiện các pha chèn góc.
   * **`ATTACK_FLANK`:** Tấn công sườn lượn vòng khi đối thủ ở tầm trung (30-45cm).
   * **`ATTACK_RETREAT_TURN`:** Một chiến thuật đặc biệt để lùi nhanh và quay 45 độ ngay sau những pha đẩy `PUSH`, tránh việc tự lao ra khỏi sàn khi bị mất phương hướng biên.

## Hướng phát triển cho V2 (Future Improvements)

* Nâng cấp hệ thống thiết kế CAD 3D chỉn chu hơn trên Fusion 360 hoặc Tinkercad thay vì chỉ cắt vẽ thủ công trên PVC.
* Arduino Uno R3 với giới hạn 32KB Flash và 2KB RAM  đôi khi gây thắt cổ chai khi xử lý nhiều tín hiệu ngắt (interrupt) từ cảm biến. Chuyển sang lập trình hệ thống trên vi điều khiển STM32 sẽ là bước nhảy vọt về tốc độ xử lý đa luồng.
* Trang bị lại cảm biến quang/IR chất lượng cao hơn để tối ưu hóa khả năng bám sàn.

## Lưu ý

* Nếu copy paste code thì cần chỉnh sửa lại thông số và debug lại các giá trị const
* File nếu không có IR được chỉnh sửa ngay ở thực chiến -> có thể sẽ có logic tốt hơn bản kia một chút
---
*Dự án được phân phối dưới giấy phép MIT License.*
