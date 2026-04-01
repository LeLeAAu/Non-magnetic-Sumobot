#include <NewPing.h>

// === ĐỊNH NGHĨA ENUM ===
enum EscapeState : uint8_t { ESC_NONE };
// Đã thêm ATTACK_RETREAT_TURN (lùi và quay), ATTACK_CONTINUE (tiếp tục tấn công)
enum AttackState : uint8_t { ATK_NONE, ATTACK_FORWARD, ATTACK_PUSH, TURN_FRONT, TURN_AROUND, ATTACK_CORNER, ATTACK_FLANK, ATTACK_RETREAT_TURN, ATTACK_CONTINUE };
enum SearchState : uint8_t { SEARCH_NONE, SEARCH_LOCAL_SCAN, SEARCH_CRUISE, STARTUP_SPIN }; 

// === KHAI BÁO CHÂN ĐIỀU KHIỂN ĐỘNG CƠ ===
#define LPWM_A 5    // Động cơ trái (PWM)
#define RPWM_A 6    // Động cơ trái (PWM)
#define R_EN_A 2    // Chân ENABLE phải của cầu H (cho động cơ trái)
#define L_EN_A 3    // Chân ENABLE trái của cầu H (cho động cơ trái)
#define LPWM_B 11   // Động cơ phải (PWM)
#define RPWM_B 4    // Động cơ phải (PWM)
#define R_EN_B 7    // Chân ENABLE phải của cầu H (cho động cơ phải)
#define L_EN_B 8    // Chân ENABLE trái của cầu H (cho động cơ phải)

// === KHAI BÁO CẢM BIẾN SIÊU ÂM ===
#define TRIG_FRONT 12
#define ECHO_FRONT 13
#define TRIG_LEFT 9
#define ECHO_LEFT 10
#define TRIG_RIGHT 0
#define ECHO_RIGHT 1
#define TRIG_BACK A5 
#define ECHO_BACK A4 

#define MAX_DISTANCE 100 // Khoảng cách tối đa mà cảm biến có thể đọc (cm)
#define MIN_DISTANCE 2   // Khoảng cách tối thiểu mà cảm biến có thể đọc (cm)

// === ĐIỀU CHỈNH KHOẢNG CÁCH US ===
#define FRONT_OFFSET 3 // Bù trừ khoảng cách cho cảm biến trước
#define SIDE_OFFSET 1  // Bù trừ khoảng cách cho cảm biến bên

// Khởi tạo đối tượng siêu âm
NewPing sonar_front(TRIG_FRONT, ECHO_FRONT, MAX_DISTANCE);
NewPing sonar_left(TRIG_LEFT, ECHO_LEFT, MAX_DISTANCE);
NewPing sonar_right(TRIG_RIGHT, ECHO_RIGHT, MAX_DISTANCE);
NewPing sonar_back(TRIG_BACK, ECHO_BACK, MAX_DISTANCE); 

// === DỮ LIỆU CẢM BIẾN ===
struct SensorData {
  uint8_t value;   // Giá trị khoảng cách đọc được (cm)
  bool isValid : 1; // Cờ báo hiệu giá trị có hợp lệ không
};

SensorData us_sensors[4] = {{0, false}, {0, false}, {0, false}, {0, false}}; 

// === QUẢN LÝ TRẠNG THÁI ===
struct RobotState {
  EscapeState escape : 8; // Trạng thái thoát hiểm
  AttackState attack : 8; // Trạng thái tấn công
  SearchState search : 8; // Trạng thái tìm kiếm
  bool opponentLocked : 1; // Cờ báo hiệu có đang khóa đối thủ không
  uint16_t timer;          // Bộ đếm thời gian cho các trạng thái
  uint8_t searchPhase;     // Pha tìm kiếm/hướng quay/flank
};

RobotState currentState = { ESC_NONE, ATK_NONE, SEARCH_NONE, false, 0, 0 }; 
unsigned long lastOpponentSeen = 0; // Thời điểm cuối cùng thấy đối thủ
bool initialSearchStarted = false;  // Cờ báo hiệu tìm kiếm ban đầu đã bắt đầu chưa

// === TỐC ĐỘ ĐỘNG CƠ ===
#define MAX_SPEED 220    // Tốc độ tối đa
#define PUSH_SPEED 255   // Tốc độ khi đẩy đối thủ
#define TURN_SPEED 220   // Tốc độ khi quay tại chỗ
#define BACK_SPEED 190   // Tốc độ khi lùi

// Tốc độ cho pha quay ban đầu
#define STARTUP_SPIN_SPEED 180 

// Tốc độ cho tấn công sườn (ATTACK_FLANK)
#define FLANK_SPEED_FORWARD 200  // Tốc độ của bánh xe bên ngoài (tiến)
#define FLANK_SPEED_TURN 80      // Tốc độ của bánh xe bên trong (tiến, nhưng chậm hơn để tạo vòng cung)

// Tốc độ cho di chuyển vòng tròn nhỏ (SEARCH_CRUISE)
#define SEARCH_CRUISE_SPEED_FORWARD 150 // Tốc độ tiến của bánh xe nhanh hơn trong vòng tròn nhỏ
#define SEARCH_CRUISE_SPEED_TURN 50     // Tốc độ tiến của bánh xe chậm hơn để tạo vòng cung

#define ATTACK_CORNER_SPEED_FORWARD 180 // Tốc độ tiến của bánh xe bên ngoài khi tấn công góc
#define ATTACK_CORNER_SPEED_TURN 80     // Tốc độ tiến của bánh xe bên trong (thấp hơn để tạo góc cua)

// === THỜI GIAN TRẠNG THÁI (ms) ===
#define TURN_DURATION 200               // Thời gian quay tìm đối thủ gần
#define TURN_AROUND_DURATION 450        // Thời gian xoay 180 độ khi đối thủ ở sau. CẦN HIỆU CHỈNH!
#define OPPONENT_MEMORY 1000            // Thời gian ghi nhớ đối thủ (ms)

// Thời gian cho các pha trong SEARCH_LOCAL_SCAN (chỉ còn quay)
#define LOCAL_SEARCH_TURN_DURATION 200 // Thời gian quay trong quét cục bộ

#define CORNER_ATTACK_THRESHOLD 15      // Ngưỡng khoảng cách để kích hoạt tấn công góc (cm)

#define ATTACK_CORNER_DURATION 300      // Thời gian tấn công góc (ms)
#define STARTUP_SPIN_DURATION 500       // Thời gian quay ban đầu sau 3s chờ (có thể điều chỉnh)

// Ngưỡng khoảng cách cho chiến thuật FLANK
#define FLANK_MIN_DISTANCE 30  // Khoảng cách tối thiểu để bắt đầu flank (tầm trung)
#define FLANK_MAX_DISTANCE 45  // Khoảng cách tối đa để bắt đầu flank (tầm trung)
#define FLANK_DURATION 600     // Thời gian thực hiện flank (ms). CẦN HIỆU CHỈNH!

// Thời gian cho mỗi pha trong SEARCH_CRUISE (vòng tròn nhỏ)
#define SMALL_CIRCLE_DURATION 500 // Thời gian robot di chuyển theo một cung tròn (sang trái hoặc phải). CẦN HIỆU CHỈNH!

// Các định nghĩa mới cho hành vi lùi và quay 45 độ
#define RETREAT_BACK_DURATION 200 // Thời gian lùi sau khi đẩy (ms). CẦN HIỆU CHỈNH!
#define RETREAT_TURN_DURATION 150 // Thời gian quay 45 độ (ms). CẦN HIỆU CHỈNH! (Khoảng 1/4 của TURN_AROUND_DURATION)
#define RETREAT_TURN_SPEED 200    // Tốc độ quay khi lùi và xoay.

// === TỐC ĐỘ THÍCH ỨNG (LOOKUP TABLE) ===
// Chỉ áp dụng khi tấn công thẳng, không dùng khi tìm kiếm
const uint8_t ADAPTIVE_SPEED_SLOW = 180; // Tốc độ khi đối thủ còn hơi xa (dưới 15cm)
const uint8_t ADAPTIVE_SPEED_MEDIUM = 220; // Tốc độ khi đối thủ gần hơn (dưới 10cm)
const uint8_t ADAPTIVE_SPEED_FAST = PUSH_SPEED; // Tốc độ khi đối thủ rất gần (dưới 5cm)

inline uint8_t calculateAdaptiveSpeed(uint8_t distance) {
  if (distance <= 14) return ADAPTIVE_SPEED_FAST;
  if (distance <= 17) return ADAPTIVE_SPEED_MEDIUM;
  if (distance <= 24) return ADAPTIVE_SPEED_SLOW;
  return 0; // Không tiến nếu đối thủ quá xa
}

// === HÀM ĐỌC SIÊU ÂM ===
inline uint8_t readOptimizedUS(NewPing& sonar, SensorData& sensor, uint8_t offset) {
  uint8_t dist = sonar.ping_cm();
  if (dist >= MIN_DISTANCE && dist >= 0 && dist <= MAX_DISTANCE) { // Đảm bảo dist không âm
    sensor.value = dist - offset; 
    sensor.isValid = true;
    return sensor.value;
  }
  sensor.value = 0; 
  sensor.isValid = false; 
  return sensor.value;
}

// === ĐIỀU KHIỂN ĐỘNG CƠ ===
inline void moveForward(uint8_t speed) {
  analogWrite(LPWM_A, speed); analogWrite(RPWM_A, 0); // Bánh trái tiến
  analogWrite(LPWM_B, 0); analogWrite(RPWM_B, speed); // Bánh phải tiến
}

inline void moveBackward(uint8_t speed) { 
  analogWrite(LPWM_A, 0); analogWrite(RPWM_A, speed); // Bánh trái lùi
  analogWrite(LPWM_B, speed); analogWrite(RPWM_B, 0); // Bánh phải lùi
}

// Quay trái tại chỗ: động cơ trái lùi, động cơ phải tiến
inline void spinLeft(uint8_t speed) { 
  analogWrite(LPWM_A, 0); analogWrite(RPWM_A, speed); 
  analogWrite(LPWM_B, 0); analogWrite(RPWM_B, speed); 
}

// Quay phải tại chỗ: động cơ phải lùi, động cơ trái tiến
inline void spinRight(uint8_t speed) { 
  analogWrite(LPWM_A, speed); analogWrite(RPWM_A, 0); 
  analogWrite(LPWM_B, speed); analogWrite(RPWM_B, 0); 
}

// Xoay 180 độ: một motor tiến, một motor lùi (spin)
inline void spinAround(uint8_t speed) {
    spinLeft(speed); // Có thể chọn spinLeft hoặc spinRight
}

inline void stopMotors() {
  analogWrite(LPWM_A, 0); analogWrite(RPWM_A, 0);
  analogWrite(LPWM_B, 0); analogWrite(RPWM_B, 0);
}

// Di chuyển theo vòng cung nhỏ sang trái (bánh trái chậm, bánh phải nhanh)
inline void moveSmallCircleLeft(uint8_t speed_forward, uint8_t speed_turn) {
  analogWrite(LPWM_A, speed_turn); analogWrite(RPWM_A, 0);         // Bánh trái tiến chậm
  analogWrite(LPWM_B, 0); analogWrite(RPWM_B, speed_forward);   // Bánh phải tiến nhanh
}

// Di chuyển theo vòng cung nhỏ sang phải (bánh trái nhanh, bánh phải chậm)
inline void moveSmallCircleRight(uint8_t speed_forward, uint8_t speed_turn) {
  analogWrite(LPWM_A, speed_forward); analogWrite(RPWM_A, 0);   // Bánh trái tiến nhanh
  analogWrite(LPWM_B, 0); analogWrite(RPWM_B, speed_turn);      // Bánh phải tiến chậm
}

// === CẬP NHẬT CẢM BIẾN ===
void updateSensors() {
  static uint8_t usCycle = 0;
  switch (usCycle) {
    case 0: readOptimizedUS(sonar_front, us_sensors[0], FRONT_OFFSET); break;
    case 1: readOptimizedUS(sonar_left, us_sensors[1], SIDE_OFFSET); break;
    case 2: readOptimizedUS(sonar_right, us_sensors[2], SIDE_OFFSET); break;
    case 3: 
            readOptimizedUS(sonar_back, us_sensors[3], 0); 
            break;
  }
  usCycle = (usCycle + 1) & 0x03; // Chuyển đổi giữa 0, 1, 2, 3
}

// === PHÁT HIỆN ĐỐI THỦ ===
inline bool detectOpponent() {
  // Đối thủ được coi là phát hiện nếu bất kỳ cảm biến nào thấy trong phạm vi nhất định
  // Cảm biến trước, trái, phải trong 24-26cm; Cảm biến sau trong 17cm
  bool opponent = (us_sensors[0].isValid && us_sensors[0].value > 0 && us_sensors[0].value < 24) ||
                  (us_sensors[1].isValid && us_sensors[1].value > 0 && us_sensors[1].value < 26) ||
                  (us_sensors[2].isValid && us_sensors[2].value > 0 && us_sensors[2].value < 26) ||
                  (us_sensors[3].isValid && us_sensors[3].value > 0 && us_sensors[3].value < 17); 
  if (opponent) lastOpponentSeen = millis(); // Cập nhật thời gian cuối cùng thấy đối thủ
  // Nếu đã quá lâu không thấy đối thủ, reset cờ opponentLocked
  if (millis() - lastOpponentSeen > OPPONENT_MEMORY) currentState.opponentLocked = false;
  return opponent;
}

// === CHIẾN LƯỢC ===
void strategy() {
  currentState.escape = ESC_NONE; 

  // Nếu timer đã hết, reset trạng thái hoặc chuyển pha
  if (currentState.timer == 0) {
    // Nếu vừa kết thúc ATTACK_RETREAT_TURN, chuyển sang ATTACK_CONTINUE
    if (currentState.attack == ATTACK_RETREAT_TURN) {
      currentState.attack = ATTACK_CONTINUE; // Sau khi lùi và quay, tiếp tục tấn công thẳng
      currentState.timer = 50; // Thời gian ngắn để thực hiện ATTACK_CONTINUE rồi đánh giá lại
      return; 
    }

    // Nếu vừa kết thúc ATTACK_PUSH hoặc ATTACK_FORWARD
    // và đối thủ vẫn còn rất gần (ngưỡng 10cm sau khi đẩy)
    if ((currentState.attack == ATTACK_PUSH || currentState.attack == ATTACK_FORWARD) &&
        us_sensors[0].isValid && us_sensors[0].value > 0 && us_sensors[0].value <= 10) {
      currentState.attack = ATTACK_RETREAT_TURN;
      currentState.searchPhase = random(2); // 0 cho trái, 1 cho phải
      currentState.timer = RETREAT_BACK_DURATION + RETREAT_TURN_DURATION; // Tổng thời gian lùi và quay
      return; 
    }

    // Nếu vừa kết thúc ATTACK_CONTINUE, reset trạng thái attack về ATK_NONE.
    // KHÔNG return ở đây để robot có thể tiếp tục chạy qua logic phát hiện đối thủ ngay lập tức
    // và kích hoạt lại các trạng thái tấn công nếu đối thủ vẫn ở đó.
    if (currentState.attack == ATTACK_CONTINUE) {
        currentState.attack = ATK_NONE; 
    } else {
        currentState.attack = ATK_NONE; // Reset trạng thái tấn công nếu không có hành động đặc biệt
    }
    
    // Logic chuyển pha tìm kiếm (chỉ thực hiện nếu không có trạng thái tấn công nào đang hoạt động)
    if (currentState.attack == ATK_NONE) { 
      if (currentState.search == SEARCH_LOCAL_SCAN) {
        currentState.searchPhase = (currentState.searchPhase + 1) % 2; 
        currentState.timer = LOCAL_SEARCH_TURN_DURATION;
      } 
      else if (currentState.search == SEARCH_CRUISE) {
        currentState.searchPhase = (currentState.searchPhase + 1) % 2; 
        currentState.timer = SMALL_CIRCLE_DURATION; 
      }
    }
  }

  // Logic khởi đầu sau 3 giây chờ
  if (!initialSearchStarted) {
    currentState.search = STARTUP_SPIN; 
    currentState.timer = STARTUP_SPIN_DURATION;    
    initialSearchStarted = true;    
    return; 
  }

  // Kiểm tra xem robot có đang thực hiện một cú quay nhanh (TURN_FRONT/TURN_AROUND)
  // hoặc đang trong pha lùi-quay
  bool isTurningFast = (currentState.attack == TURN_FRONT || 
                        currentState.attack == TURN_AROUND ||
                        currentState.attack == ATTACK_RETREAT_TURN); 

  // Nếu robot đang trong trạng thái lùi-quay, tiếp tục thực hiện hành động đó
  if (currentState.attack == ATTACK_RETREAT_TURN) {
    return; // Không thay đổi trạng thái nếu đang trong trạng thái này
  }
  // Nếu đang ở ATTACK_CONTINUE, cho phép nó tiếp tục chạy qua logic phát hiện đối thủ
  // để quyết định hành động tiếp theo, không return.

  // Nếu phát hiện đối thủ HOẶC đang ở ATTACK_CONTINUE (để tiếp tục chuỗi tấn công sau khi quay)
  if (detectOpponent() || currentState.attack == ATTACK_CONTINUE) { 
    currentState.opponentLocked = true; 
    currentState.search = SEARCH_NONE;  

    // Logic Tấn công góc: ưu tiên cao nhất
    if (us_sensors[0].isValid && us_sensors[0].value < CORNER_ATTACK_THRESHOLD &&
        us_sensors[1].isValid && us_sensors[1].value < CORNER_ATTACK_THRESHOLD &&
        us_sensors[1].value <= us_sensors[0].value) { 
      currentState.attack = ATTACK_CORNER;
      currentState.timer = ATTACK_CORNER_DURATION;
      currentState.searchPhase = 0; // Quay sang TRÁI 
      return;
    } 
    else if (us_sensors[0].isValid && us_sensors[0].value < CORNER_ATTACK_THRESHOLD &&
             us_sensors[2].isValid && us_sensors[2].value < CORNER_ATTACK_THRESHOLD &&
             us_sensors[2].value <= us_sensors[0].value) { 
      currentState.attack = ATTACK_CORNER;
      currentState.timer = ATTACK_CORNER_DURATION;
      currentState.searchPhase = 1; // Quay sang PHẢI 
      return;
    }

    // Logic Tấn công sườn (Flank): khi đối thủ ở tầm trung
    if (us_sensors[0].isValid && us_sensors[0].value >= FLANK_MIN_DISTANCE && us_sensors[0].value <= FLANK_MAX_DISTANCE) {
        if (us_sensors[1].isValid && us_sensors[2].isValid) {
            if (us_sensors[1].value < us_sensors[2].value) { // Đối thủ lệch trái, flank sang trái
                currentState.attack = ATTACK_FLANK;
                currentState.timer = FLANK_DURATION;
                currentState.searchPhase = 0; 
                return;
            } else if (us_sensors[2].isValid && us_sensors[2].value < us_sensors[1].value) { // Đối thủ lệch phải, flank sang phải
                currentState.attack = ATTACK_FLANK;
                currentState.timer = FLANK_DURATION;
                currentState.searchPhase = 1; 
                return;
            }
        } 
        else if (us_sensors[1].isValid && us_sensors[1].value >= FLANK_MIN_DISTANCE && us_sensors[1].value <= FLANK_MAX_DISTANCE) {
            currentState.attack = ATTACK_FLANK;
            currentState.timer = FLANK_DURATION;
            currentState.searchPhase = 0; 
            return;
        }
        else if (us_sensors[2].isValid && us_sensors[2].value >= FLANK_MIN_DISTANCE && us_sensors[2].value <= FLANK_MAX_DISTANCE) {
            currentState.attack = ATTACK_FLANK;
            currentState.timer = FLANK_DURATION;
            currentState.searchPhase = 1; 
            return;
        }
    }

    // Logic Tấn công thẳng (PUSH/FORWARD): Ưu tiên cao
    // Nếu đối thủ còn rất gần (<= 24cm), hãy tấn công thẳng.
    // Điều này sẽ cho phép robot chuyển từ ATTACK_CONTINUE trở lại PUSH/FORWARD nếu đối thủ vẫn ở trước mặt
    // sau cú quay 45 độ, tạo thành chuỗi lặp lại.
    if (us_sensors[0].isValid && us_sensors[0].value > 0 && us_sensors[0].value <= 24) { 
        uint8_t adaptiveSpeed = calculateAdaptiveSpeed(us_sensors[0].value);
        if (adaptiveSpeed > 0) {
            currentState.attack = (adaptiveSpeed == PUSH_SPEED) ? ATTACK_PUSH : ATTACK_FORWARD;
            currentState.timer = 500; // Thời gian duy trì đẩy/tiến
            return;
        }
    } 
    // Các logic quay khác (TURN_FRONT, TURN_AROUND)
    else if (us_sensors[1].isValid && us_sensors[1].value > 0 && us_sensors[1].value < 26) { 
      currentState.attack = TURN_FRONT;      
      currentState.timer = TURN_DURATION;
      return;
    } else if (us_sensors[2].isValid && us_sensors[2].value > 0 && us_sensors[2].value < 26) { 
      currentState.attack = TURN_FRONT;      
      currentState.timer = TURN_DURATION;
      return;
    } 
    else if (us_sensors[3].isValid && us_sensors[3].value > 0 && us_sensors[3].value < 17) { 
      currentState.attack = TURN_AROUND;      
      currentState.timer = TURN_AROUND_DURATION; 
      return;
    }
    currentState.attack = ATK_NONE; 
  } 

  // Nếu không trong trạng thái tấn công và không đang quay nhanh để tấn công
  // Robot sẽ chuyển sang tìm kiếm
  if (!isTurningFast && !currentState.opponentLocked) { 
    currentState.attack = ATK_NONE; 
    
    if (currentState.timer == 0 || currentState.search == SEARCH_NONE) {
        if (currentState.search == SEARCH_LOCAL_SCAN) {
             currentState.search = SEARCH_CRUISE; 
             currentState.searchPhase = random(2); 
             currentState.timer = SMALL_CIRCLE_DURATION; 
        } else if (currentState.search == SEARCH_CRUISE) {
             currentState.searchPhase = (currentState.searchPhase + 1) % 2; 
             currentState.timer = SMALL_CIRCLE_DURATION;
        }
        else { 
             currentState.search = SEARCH_LOCAL_SCAN; 
             currentState.searchPhase = random(2); 
             currentState.timer = LOCAL_SEARCH_TURN_DURATION; 
        }
    }
  }
}

// === CẬP NHẬT TRẠNG THÁI ===
void updateState() {
  if (currentState.timer) currentState.timer--; 

  switch (currentState.attack) {
    case ATTACK_FORWARD: 
      moveForward(calculateAdaptiveSpeed(us_sensors[0].value)); 
      break; 
    case ATTACK_PUSH: 
      moveForward(PUSH_SPEED); 
      break;
    
    // === TRẠNG THÁI MỚI: LÙI VÀ QUAY 45 ĐỘ ===
    case ATTACK_RETREAT_TURN:
      // Kiểm tra xem thời gian lùi đã kết thúc hay chưa
      if (currentState.timer > RETREAT_TURN_DURATION) { 
        moveBackward(BACK_SPEED); // Lùi
      } else { // Chuyển sang pha quay 45 độ
        if (currentState.searchPhase == 0) { // Quay trái
          spinLeft(RETREAT_TURN_SPEED);
        } else { // Quay phải
          spinRight(RETREAT_TURN_SPEED);
        }
      }
      break;

    // === TRẠNG THÁI MỚI: TIẾP TỤC TẤN CÔNG SAU KHI LÙI VÀ QUAY ===
    case ATTACK_CONTINUE:
      // Tiếp tục tấn công thẳng theo hướng hiện tại
      moveForward(PUSH_SPEED); 
      break;

    case TURN_FRONT: 
        if (us_sensors[1].isValid && us_sensors[2].isValid) {
            (us_sensors[1].value < us_sensors[2].value) ? spinLeft(TURN_SPEED) : spinRight(TURN_SPEED); 
        } else if (us_sensors[1].isValid) {
            spinLeft(TURN_SPEED);
        } else if (us_sensors[2].isValid) {
            spinRight(TURN_SPEED);
        } else { 
            if (random(2) == 0) spinLeft(TURN_SPEED); else spinRight(TURN_SPEED);
        }
        break; 
    case TURN_AROUND: 
        spinAround(TURN_SPEED); 
        break; 
    case ATTACK_CORNER:
        if (currentState.searchPhase == 0) { 
            analogWrite(LPWM_A, ATTACK_CORNER_SPEED_TURN); analogWrite(RPWM_A, 0); 
            analogWrite(LPWM_B, 0); analogWrite(RPWM_B, ATTACK_CORNER_SPEED_FORWARD); 
        } else { 
            analogWrite(LPWM_A, ATTACK_CORNER_SPEED_FORWARD); analogWrite(RPWM_A, 0);
            analogWrite(LPWM_B, 0); analogWrite(RPWM_B, ATTACK_CORNER_SPEED_TURN);
        }
        break;
    case ATTACK_FLANK:
        if (currentState.searchPhase == 0) { 
            analogWrite(LPWM_A, FLANK_SPEED_TURN); analogWrite(RPWM_A, 0);       
            analogWrite(LPWM_B, 0); analogWrite(RPWM_B, FLANK_SPEED_FORWARD);  
        } else { 
            analogWrite(LPWM_A, FLANK_SPEED_FORWARD); analogWrite(RPWM_A, 0);  
            analogWrite(LPWM_B, 0); analogWrite(RPWM_B, FLANK_SPEED_TURN);    
        }
        break;
    default: // Không trong trạng thái tấn công
      if (currentState.search == SEARCH_LOCAL_SCAN) {
          switch (currentState.searchPhase) {
              case 0: spinRight(TURN_SPEED); break; 
              case 1: spinLeft(TURN_SPEED); break;  
          }
      } 
      else if (currentState.search == STARTUP_SPIN) { 
        spinLeft(STARTUP_SPIN_SPEED); 
      }
      else if (currentState.search == SEARCH_CRUISE) {
          if (currentState.searchPhase == 0) { 
              moveSmallCircleLeft(SEARCH_CRUISE_SPEED_FORWARD, SEARCH_CRUISE_SPEED_TURN);
          } else { 
              moveSmallCircleRight(SEARCH_CRUISE_SPEED_FORWARD, SEARCH_CRUISE_SPEED_TURN);
          }
      } else {
          stopMotors(); // Dừng nếu không có trạng thái nào được kích hoạt
      }
  }
}

void setup() {
  // Cấu hình chân điều khiển động cơ là OUTPUT
  pinMode(LPWM_A, OUTPUT); pinMode(RPWM_A, OUTPUT);
  pinMode(R_EN_A, OUTPUT); pinMode(L_EN_A, OUTPUT);
  pinMode(LPWM_B, OUTPUT); pinMode(RPWM_B, OUTPUT);
  pinMode(R_EN_B, OUTPUT); pinMode(L_EN_B, OUTPUT);

  // Kích hoạt các chân ENABLE của cầu H (mức HIGH để động cơ hoạt động)
  digitalWrite(R_EN_A, HIGH); digitalWrite(L_EN_A, HIGH);
  digitalWrite(R_EN_B, HIGH); digitalWrite(L_EN_B, HIGH);
  
  randomSeed(analogRead(A7)); // Khởi tạo seed cho hàm random (đọc từ chân analog trống)
  stopMotors(); // Đảm bảo động cơ dừng khi khởi động
  
  delay(3000); // Chờ 3 giây theo luật thi đấu
}

void loop() {
  updateSensors(); // Cập nhật dữ liệu từ các cảm biến
  strategy();      // Quyết định chiến thuật dựa trên dữ liệu cảm biến và trạng thái
  updateState();   // Thực hiện hành động dựa trên chiến thuật đã chọn

  delayMicroseconds(100); // Một khoảng trễ nhỏ để tránh quá tải CPU
}