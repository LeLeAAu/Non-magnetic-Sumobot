#include <NewPing.h>

// === ĐỊNH NGHĨA ENUM ===
enum EscapeState : uint8_t { ESC_NONE, ESCAPE_FRONT, ESCAPE_BACK, ESCAPE_SPIN, 
                             ESCAPE_TURN_AFTER_BACK }; 
enum AttackState : uint8_t { ATK_NONE, ATTACK_FORWARD, ATTACK_PUSH, TURN_FRONT, TURN_AROUND };
// Chỉ sử dụng SEARCH_LOCAL_SCAN và SEARCH_CRUISE cho trạng thái tìm kiếm khi không có đối thủ
enum SearchState : uint8_t { SEARCH_NONE, SEARCH_LOCAL_SCAN, SEARCH_CRUISE }; 

// === KHAI BÁO CHÂN ĐIỀU KHIỂN ĐỘNG CƠ ===
#define LPWM_A 5    // Động cơ trái
#define RPWM_A 6
#define R_EN_A 2
#define L_EN_A 3
#define LPWM_B 11   // Động cơ phải
#define RPWM_B 4
#define R_EN_B 7
#define L_EN_B 8

// === KHAI BÁO CẢM BIẾN SIÊU ÂM (Bỏ cảm biến sau) ===
#define TRIG_FRONT 12
#define ECHO_FRONT 13
#define TRIG_LEFT 9
#define ECHO_LEFT 10
#define TRIG_RIGHT 0
#define ECHO_RIGHT 1

#define MAX_DISTANCE 100 
#define MIN_DISTANCE 2  

// === ĐIỀU CHỈNH KHOẢNG CÁCH US ===
#define FRONT_OFFSET 4
#define SIDE_OFFSET 1

// Ngưỡng phát hiện biên bằng cảm biến siêu âm
#define US_BORDER_THRESHOLD_FRONT 10 
#define US_BORDER_THRESHOLD_SIDE 8  

// Khởi tạo đối tượng siêu âm (Chỉ 3 cảm biến: trước, trái, phải)
NewPing sonar_front(TRIG_FRONT, ECHO_FRONT, MAX_DISTANCE);
NewPing sonar_left(TRIG_LEFT, ECHO_LEFT, MAX_DISTANCE);
NewPing sonar_right(TRIG_RIGHT, ECHO_RIGHT, MAX_DISTANCE);

// === DỮ LIỆU CẢM BIẾN ===
struct SensorData {
  uint8_t value; 
  bool isValid : 1; 
};

struct IRData { // Cấu trúc dữ liệu cho cảm biến IR
  uint8_t value; 
  uint8_t state;
};

// us_sensors bây giờ chỉ có 3 phần tử (0=Front, 1=Left, 2=Right)
SensorData us_sensors[3] = {{0, false}, {0, false}, {0, false}}; 
IRData ir_sensors[4]; // FL (A0), FR (A1), BL (A2), BR (A3)
const uint8_t IR_PINS[] PROGMEM = {A0, A1, A2, A3}; // FL, FR, BL, BR

// === QUẢN LÝ TRẠNG THÁI ===
struct RobotState {
  EscapeState escape : 8; 
  AttackState attack : 8; 
  SearchState search : 8;
  bool opponentLocked : 1;
  uint16_t timer;
  uint8_t searchPhase; // Giữ lại searchPhase cho SEARCH_LOCAL_SCAN
};

// Khởi tạo trạng thái ban đầu.
RobotState currentState = { ESC_NONE, ATK_NONE, SEARCH_NONE, false, 0, 0 }; 
unsigned long lastOpponentSeen = 0;
bool initialSearchStarted = false; 

// === NGƯỠNG CẢM BIẾN IR ===
// Với điều kiện: IR>9: đen, IR<8: trắng
// Vậy ngưỡng để xác định màu trắng sẽ là <= 7 (hoặc bất kỳ giá trị nào nhỏ hơn 8)
// Ngưỡng để xác định màu đen sẽ là >= 10 (hoặc bất kỳ giá trị nào lớn hơn 9)
#define IR_WHITE_THRESHOLD 9 // Giá trị đọc được nhỏ hơn hoặc bằng 7 là màu trắng (biên)
#define IR_BLACK_THRESHOLD 10 // Giá trị đọc được lớn hơn hoặc bằng 10 là màu đen (sàn)

// === TỐC ĐỘ ĐỘNG CƠ ===
#define MAX_SPEED 220
#define PUSH_SPEED 255
#define TURN_SPEED 220 
#define BACK_SPEED 190 // Tốc độ lùi khi chạm biên
#define LOCAL_SEARCH_FORWARD_SPEED 100 // Tốc độ tiến khi tìm kiếm cục bộ
#define LOCAL_SEARCH_TURN_SPEED 120    // Tốc độ quay khi tìm kiếm cục bộ
#define SEARCH_CRUISE_SPEED 100        // Tốc độ đi chậm khi không thấy đối thủ
#define SLOW_SPEED 150 // Giá trị cho speedTable (có thể điều chỉnh)

// === THỜI GIAN TRẠNG THÁI (ms) ===
#define ESCAPE_BACK_DURATION 150 // Thời gian lùi khi chạm biên trước
#define ESCAPE_FORWARD_DURATION 150 // Thời gian tiến khi chạm biên sau
#define ESCAPE_SPIN_DURATION 300 // Thời gian quay tại chỗ khi chạm biên bên
#define ESCAPE_FINAL_TURN_DURATION 100 // Thời gian quay sau khi lùi/tiến tránh biên

#define TURN_DURATION 200 
#define TURN_AROUND_DURATION 400 
#define OPPONENT_MEMORY 1000 

// Thời gian cho các pha tìm kiếm cục bộ và cruise
#define PHASE_FORWARD_DURATION 500  
#define PHASE_BACKWARD_DURATION 300 
#define PHASE_TURN_DURATION 200    
#define PHASE_CRUISE_DURATION 2000 // Thời gian cho SEARCH_CRUISE

// === TỐC ĐỘ THÍCH ỨNG (LOOKUP TABLE) ===
const uint8_t speedTable[] PROGMEM = {SLOW_SPEED, MAX_SPEED, PUSH_SPEED, PUSH_SPEED};
#define SPEED_THRESHOLD_5 5
#define SPEED_THRESHOLD_10 10
#define SPEED_THRESHOLD_15 15

inline uint8_t calculateAdaptiveSpeed(uint8_t distance) {
  if (distance <= SPEED_THRESHOLD_5) return pgm_read_byte(&speedTable[3]);
  if (distance <= SPEED_THRESHOLD_10) return pgm_read_byte(&speedTable[2]);
  if (distance <= SPEED_THRESHOLD_15) return pgm_read_byte(&speedTable[1]);
  return pgm_read_byte(&speedTable[0]);
}

// === HÀM ĐỌC SIÊU ÂM ===
inline uint8_t readOptimizedUS(NewPing& sonar, SensorData& sensor, uint8_t offset) {
  uint8_t dist = sonar.ping_cm();
  if (dist >= MIN_DISTANCE && dist <= MAX_DISTANCE) {
    sensor.value = dist - offset; 
    sensor.isValid = true;
    return sensor.value;
  }
  sensor.value = 0; 
  sensor.isValid = false; 
  return sensor.value;
}

// === HÀM ĐỌC IR ===
inline void readOptimizedIR() {
  for (uint8_t i = 0; i < 4; i++) {
    uint8_t reading = analogRead(pgm_read_byte(&IR_PINS[i])) >> 2;
    ir_sensors[i].value = reading; 

    ir_sensors[i].state = 0; 

    // Logic mới cho IR: IR<8 là trắng (biên), IR>9 là đen (sàn)
    if (ir_sensors[i].value < IR_BLACK_THRESHOLD) { // Nếu giá trị < 10, có thể là trắng
      if (ir_sensors[i].value <= IR_WHITE_THRESHOLD) { // Nếu giá trị <= 7, chắc chắn là trắng
        ir_sensors[i].state |= 0x01; // Bit 0: Đang ở biên (màu trắng)
      }
    }
    // Nếu giá trị lớn hơn hoặc bằng IR_BLACK_THRESHOLD (10), thì không đặt bit 0 (biên)
    // Nếu giá trị nằm giữa 8 và 9 (ví dụ 8 hoặc 9), thì không đặt bit 0 và không coi là đen rõ ràng,
    // coi như không có biên (có thể là vùng xám hoặc nhiễu)
  }
}

// === ĐIỀU KHIỂN ĐỘNG CƠ ===
inline void moveForward(uint8_t speed) {
  analogWrite(LPWM_A, speed); analogWrite(RPWM_A, 0);
  analogWrite(LPWM_B, 0); analogWrite(RPWM_B, speed);
}

inline void moveBackward(uint8_t speed) { 
  analogWrite(LPWM_A, 0); analogWrite(RPWM_A, speed);
  analogWrite(LPWM_B, speed); analogWrite(RPWM_B, 0);
}

inline void turnLeft(uint8_t speed) { 
  analogWrite(LPWM_A, 0); analogWrite(RPWM_A, speed);
  analogWrite(LPWM_B, 0); analogWrite(RPWM_B, speed);
}

inline void turnRight(uint8_t speed) { 
  analogWrite(LPWM_A, speed); analogWrite(RPWM_A, 0);
  analogWrite(LPWM_B, speed); analogWrite(RPWM_B, 0);
}

inline void stopMotors() {
  analogWrite(LPWM_A, 0); analogWrite(RPWM_A, 0);
  analogWrite(LPWM_B, 0); analogWrite(RPWM_B, 0);
}

// === CẬP NHẬT CẢM BIẾN ===
void updateSensors() {
  readOptimizedIR(); // Đọc cảm biến IR
  static uint8_t usCycle = 0;
  switch (usCycle) {
    case 0: readOptimizedUS(sonar_front, us_sensors[0], FRONT_OFFSET); break;
    case 1: readOptimizedUS(sonar_left, us_sensors[1], SIDE_OFFSET); break;
    case 2: readOptimizedUS(sonar_right, us_sensors[2], SIDE_OFFSET); break;
  }
  usCycle = (usCycle + 1) % 3; // Chu kỳ 3 (0, 1, 2)
}

// === PHÁT HIỆN ĐỐI THỦ ===
inline bool detectOpponent() {
  // Chỉ sử dụng 3 cảm biến US phía trước để phát hiện đối thủ
  bool opponent = (us_sensors[0].isValid && us_sensors[0].value > 0 && us_sensors[0].value < 20) ||
                  (us_sensors[1].isValid && us_sensors[1].value > 0 && us_sensors[1].value < 20) ||
                  (us_sensors[2].isValid && us_sensors[2].value > 0 && us_sensors[2].value < 20);
  if (opponent) lastOpponentSeen = millis();
  if (millis() - lastOpponentSeen > OPPONENT_MEMORY) currentState.opponentLocked = false;
  return opponent;
}

// === CHIẾN LƯỢC ===
void strategy() {
  // Ưu tiên trạng thái thoát biên nếu đang hoạt động
  if (currentState.escape != ESC_NONE && currentState.timer > 0) {
      return; 
  }

  // Nếu timer của trạng thái hiện tại đã hết, hoặc thoát biên đã hoàn thành
  if (currentState.timer == 0) {
      if (currentState.escape == ESCAPE_FRONT || currentState.escape == ESCAPE_BACK) { 
          currentState.escape = ESCAPE_TURN_AFTER_BACK; 
          currentState.timer = ESCAPE_FINAL_TURN_DURATION;
          return; 
      } else if (currentState.escape == ESCAPE_TURN_AFTER_BACK || 
                 currentState.escape == ESCAPE_SPIN) {
          currentState.escape = ESC_NONE; 
      }
      currentState.attack = ATK_NONE; 
      // Reset pha tìm kiếm cục bộ khi chuyển trạng thái hoặc kết thúc tìm kiếm
      if (currentState.search == SEARCH_LOCAL_SCAN) {
          currentState.searchPhase = 0; 
      }
  }

  // Giai đoạn khởi động ban đầu. Robot sẽ dừng 3s sau đó bắt đầu SEARCH_CRUISE.
  if (!initialSearchStarted) {
    // Để đảm bảo robot không di chuyển ngay lập tức khi khởi động,
    // hãy thiết lập timer ban đầu đủ dài cho trạng thái dừng.
    // Sau khi timer này hết, SEARCH_CRUISE sẽ được kích hoạt.
    // Ví dụ: delay(3000) trong setup đã dừng robot 3s,
    // nên ở đây ta có thể bắt đầu SEARCH_CRUISE ngay.
    currentState.search = SEARCH_CRUISE; // Bắt đầu bằng đi chậm
    currentState.timer = PHASE_CRUISE_DURATION;     
    initialSearchStarted = true;     
    return; 
  }

  // Xác định liệu robot có đang quay nhanh không
  bool isTurningFast = (currentState.attack == TURN_FRONT || 
                         currentState.attack == TURN_AROUND); 

  // --- Phát hiện biên bằng IR (Ưu tiên cao hơn US và tấn công khi không thoát) ---
  if (currentState.escape == ESC_NONE && !isTurningFast) { 
    uint8_t borderCount = 0;
    // Kiểm tra tất cả các cảm biến IR
    for (uint8_t i = 0; i < 4; i++) {
      if (ir_sensors[i].state & 0x01) { // Nếu cảm biến đang ở biên (màu trắng)
        borderCount++;
      }
    }

    if (borderCount >= 3) { // 3 hoặc 4 cảm biến chạm biên: quay tại chỗ
        currentState.escape = ESCAPE_SPIN; 
        currentState.timer = ESCAPE_SPIN_DURATION;
        return;
    } 
    // Kiểm tra các trường hợp chạm biên cụ thể bằng IR
    else if ((ir_sensors[0].state & 0x01) || (ir_sensors[1].state & 0x01)) { // Nếu IR phía trước trái HOẶC trước phải chạm biên (màu trắng)
      currentState.escape = ESCAPE_FRONT; // Lùi lại
      currentState.timer = ESCAPE_BACK_DURATION; 
      return;
    } 
    else if ((ir_sensors[2].state & 0x01) || (ir_sensors[3].state & 0x01)) { // Nếu IR phía sau trái HOẶC sau phải chạm biên (màu trắng)
      currentState.escape = ESCAPE_BACK; // Tiến lên
      currentState.timer = ESCAPE_FORWARD_DURATION;
      return;
    }
  }

  // --- Phát hiện đối thủ và tấn công (Ưu tiên cao nhất sau Escape bằng IR) ---
  if (currentState.escape == ESC_NONE && detectOpponent()) { 
    currentState.opponentLocked = true; 
    currentState.search = SEARCH_NONE; // Dừng tìm kiếm khi thấy đối thủ

    if (us_sensors[0].isValid && us_sensors[0].value > 0 && us_sensors[0].value <= 10) { 
      currentState.attack = ATTACK_PUSH; // Đẩy nếu rất gần
      currentState.timer = 1000; 
    } else if (us_sensors[0].isValid && us_sensors[0].value > 0 && us_sensors[0].value < 20 && 
               (us_sensors[0].value <= us_sensors[1].value || !us_sensors[1].isValid) && 
               (us_sensors[0].value <= us_sensors[2].value || !us_sensors[2].isValid)) { 
      currentState.attack = ATTACK_FORWARD; // Tiến thẳng nếu đối thủ ở trước
      currentState.timer = 1000; 
    } else if (us_sensors[1].isValid && us_sensors[1].value > 0 && us_sensors[1].value < 20 && 
               (us_sensors[1].value < us_sensors[0].value || !us_sensors[0].isValid)) { 
      currentState.attack = TURN_FRONT; // Quay nếu đối thủ ở bên trái
      currentState.timer = TURN_DURATION;
    } else if (us_sensors[2].isValid && us_sensors[2].value > 0 && us_sensors[2].value < 20 && 
               (us_sensors[2].value < us_sensors[0].value || !us_sensors[0].isValid)) { 
      currentState.attack = TURN_FRONT; // Quay nếu đối thủ ở bên phải
      currentState.timer = TURN_DURATION;
    } 
    if (currentState.attack != ATK_NONE) return; // Nếu đã quyết định tấn công, dừng lại.
  } 

  // --- Phát hiện biên bằng US (Ưu tiên thứ ba, sau IR và tấn công) ---
  // Chỉ kiểm tra US nếu IR không phát hiện biên và không có đối thủ bị khóa
  if (currentState.escape == ESC_NONE && !isTurningFast && !currentState.opponentLocked) { 
    if (us_sensors[0].isValid && us_sensors[0].value <= US_BORDER_THRESHOLD_FRONT) { 
      currentState.escape = ESCAPE_FRONT; // Chạm biên trước
      currentState.timer = ESCAPE_BACK_DURATION; 
      return; 
    } 
    else if (us_sensors[1].isValid && us_sensors[1].value <= US_BORDER_THRESHOLD_SIDE) {
      currentState.escape = ESCAPE_SPIN; // Chạm biên trái
      currentState.timer = ESCAPE_SPIN_DURATION; 
      return; 
    }
    else if (us_sensors[2].isValid && us_sensors[2].value <= US_BORDER_THRESHOLD_SIDE) {
      currentState.escape = ESCAPE_SPIN; // Chạm biên phải
      currentState.timer = ESCAPE_SPIN_DURATION; 
      return; 
    }
  }
  
  // Nếu đã tìm thấy trạng thái thoát biên, không làm gì thêm trong strategy()
  if (currentState.escape != ESC_NONE) return; 

  // --- Tìm kiếm đối thủ khi không có đối thủ và không chạm biên (Ưu tiên thấp nhất) ---
  else { 
    currentState.attack = ATK_NONE; // Đảm bảo không ở trạng thái tấn công
    
    // Nếu timer đã hết hoặc không có trạng thái tìm kiếm hiện tại
    if (currentState.timer == 0 || currentState.search == SEARCH_NONE) {
        // Robot sẽ luân phiên giữa SEARCH_LOCAL_SCAN và SEARCH_CRUISE
        if (currentState.search == SEARCH_LOCAL_SCAN) {
             currentState.search = SEARCH_CRUISE;
             currentState.timer = PHASE_CRUISE_DURATION; 
        } else { // Nếu là SEARCH_CRUISE hoặc SEARCH_NONE
             currentState.search = SEARCH_LOCAL_SCAN;
             currentState.timer = PHASE_FORWARD_DURATION; // Bắt đầu pha 0 của Local Scan
             currentState.searchPhase = 0;
        }
    }
    
    // Logic chuyển pha của SEARCH_LOCAL_SCAN
    if (currentState.search == SEARCH_LOCAL_SCAN && currentState.timer == 0) {
        currentState.searchPhase = (currentState.searchPhase + 1) % 4; 
        switch (currentState.searchPhase) {
            case 0: 
                moveForward(LOCAL_SEARCH_FORWARD_SPEED);
                currentState.timer = PHASE_FORWARD_DURATION;
                break;
            case 1: 
                moveBackward(LOCAL_SEARCH_FORWARD_SPEED); 
                currentState.timer = PHASE_BACKWARD_DURATION;
                break;
            case 2: 
                turnRight(LOCAL_SEARCH_TURN_SPEED); 
                currentState.timer = PHASE_TURN_DURATION;
                break;
            case 3: 
                turnLeft(LOCAL_SEARCH_TURN_SPEED); 
                currentState.timer = PHASE_TURN_DURATION;
                break;
        }
    }
  }
}

// === CẬP NHẬT TRẠNG THÁI ===
void updateState() {
  if (currentState.timer) currentState.timer--; 

  // Xử lý trạng thái thoát biên (Escape)
  switch (currentState.escape) {
    case ESCAPE_FRONT:  
      moveBackward(BACK_SPEED); 
      break;
    case ESCAPE_BACK:   
      moveForward(MAX_SPEED); // Tiến với tốc độ tối đa để thoát biên sau (IR sau)
      break;
    case ESCAPE_SPIN:   
      // Chọn hướng quay dựa trên cảm biến bên nào gần biên hơn
      if (us_sensors[1].isValid && us_sensors[1].value <= US_BORDER_THRESHOLD_SIDE &&
          (!us_sensors[2].isValid || us_sensors[1].value <= us_sensors[2].value)) { 
          turnRight(TURN_SPEED); // Quay phải nếu US trái gần hơn
      } else if (us_sensors[2].isValid && us_sensors[2].value <= US_BORDER_THRESHOLD_SIDE &&
                 (!us_sensors[1].isValid || us_sensors[2].value < us_sensors[1].value)) {
          turnLeft(TURN_SPEED); // Quay trái nếu US phải gần hơn
      } else { // Trường hợp US không xác định rõ, hoặc không chạm biên US, dựa vào IR hoặc quay ngẫu nhiên
          if (ir_sensors[0].state & 0x01) { // Nếu IR trước trái chạm biên, quay phải
            turnRight(TURN_SPEED);
          } else if (ir_sensors[1].state & 0x01) { // Nếu IR trước phải chạm biên, quay trái
            turnLeft(TURN_SPEED);
          } else { // Quay ngẫu nhiên nếu không có thông tin rõ ràng
            if (random(2) == 0) turnLeft(TURN_SPEED); else turnRight(TURN_SPEED);
          }
      }
      break; 
    case ESCAPE_TURN_AFTER_BACK: 
      // Quay ngẫu nhiên sau khi lùi/tiến tránh biên
      if (random(2) == 0) { 
        turnLeft(TURN_SPEED); 
      } else {
        turnRight(TURN_SPEED);
      }
      break;
    default: // Nếu không ở trạng thái thoát biên
      // Xử lý trạng thái tấn công (Attack) hoặc tìm kiếm (Search)
      switch (currentState.attack) {
        case ATTACK_FORWARD: moveForward(calculateAdaptiveSpeed(us_sensors[0].value)); break; 
        case ATTACK_PUSH: moveForward(PUSH_SPEED); break;
        case TURN_FRONT: 
            // Quay về phía đối thủ dựa trên cảm biến bên nào gần hơn
            if (us_sensors[1].isValid && us_sensors[2].isValid) {
                (us_sensors[1].value < us_sensors[2].value) ? turnLeft(TURN_SPEED) : turnRight(TURN_SPEED); 
            } else if (us_sensors[1].isValid) {
                turnLeft(TURN_SPEED);
            } else if (us_sensors[2].isValid) {
                turnRight(TURN_SPEED);
            } else { // Nếu cả hai cảm biến bên đều không hợp lệ, quay ngẫu nhiên
                if (random(2) == 0) turnLeft(TURN_SPEED); else turnRight(TURN_SPEED);
            }
            break; 
        case TURN_AROUND: turnLeft(TURN_SPEED); break; // Vẫn giữ TURN_AROUND nhưng chỉ khi đối thủ được phát hiện bởi các cảm biến phía trước sau một khoảng thời gian không thấy đối thủ
        default: // Nếu không ở trạng thái tấn công, thực hiện tìm kiếm
          if (currentState.search == SEARCH_LOCAL_SCAN) {
              // Các pha của tìm kiếm cục bộ
              switch (currentState.searchPhase) {
                  case 0: moveForward(LOCAL_SEARCH_FORWARD_SPEED); break;
                  case 1: moveBackward(LOCAL_SEARCH_FORWARD_SPEED); break; 
                  case 2: turnRight(LOCAL_SEARCH_TURN_SPEED); break;
                  case 3: turnLeft(LOCAL_SEARCH_TURN_SPEED); break;
              }
          } else if (currentState.search == SEARCH_CRUISE) {
              moveForward(SEARCH_CRUISE_SPEED); // Robot đi chậm khi không thấy đối thủ
          } else {
              stopMotors(); // Nếu không có trạng thái nào, dừng
          }
      }
  }
}

void setup() {
  // Cài đặt chân động cơ
  pinMode(LPWM_A, OUTPUT); pinMode(RPWM_A, OUTPUT);
  pinMode(R_EN_A, OUTPUT); pinMode(L_EN_A, OUTPUT);
  pinMode(LPWM_B, OUTPUT); pinMode(RPWM_B, OUTPUT);
  pinMode(R_EN_B, OUTPUT); pinMode(L_EN_B, OUTPUT);
  digitalWrite(R_EN_A, HIGH); digitalWrite(L_EN_A, HIGH);
  digitalWrite(R_EN_B, HIGH); digitalWrite(L_EN_B, HIGH);
  
  // Cài đặt chân IR
  for (uint8_t i = 0; i < 4; i++) pinMode(pgm_read_byte(&IR_PINS[i]), INPUT);
  
  // Đọc IR khởi tạo (để có giá trị ban đầu cho randomSeed, nếu cần)
  for (uint8_t i = 0; i < 4; i++) ir_sensors[i].value = analogRead(pgm_read_byte(&IR_PINS[i])) >> 2;
  
  // Khởi tạo bộ tạo số ngẫu nhiên
  randomSeed(analogRead(A7)); 
  stopMotors(); 
  
  // Chờ 3 giây trước khi bắt đầu hoạt động
  delay(3000); 
}

void loop() {
  updateSensors(); // Cập nhật dữ liệu từ các cảm biến US và IR
  strategy();      // Quyết định hành động dựa trên trạng thái và cảm biến
  updateState();   // Thực thi hành động đã quyết định

  delayMicroseconds(100); // Khoảng dừng nhỏ để ổn định chu trình
}