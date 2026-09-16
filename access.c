#include "access.h"
#include "main.h"
#include "tm1637.h"

// ---- Настройки системы доступа -------------------------------------------

// Захардкоженный код доступа. Чтобы поменять код - меняем только эти 4 символа.
static const char ACCESS_CODE[4] = {'1', '2', '3', '4'};
#define ACCESS_CODE_LEN 4

#define LED_GRANTED_PIN   9   // PA9  - зелёный светодиод: "доступ разрешён"
#define LED_DENIED_PIN    15  // PA15 - красный светодиод: "доступ запрещён"

#define RESULT_SHOW_TIME_MS  2000  // сколько мс держим результат на экране/LED

// ---- Внутреннее состояние --------------------------------------------------

typedef enum {
  ACCESS_STATE_INPUT,   // ждём ввод очередной цифры кода
  ACCESS_STATE_RESULT   // код проверен, показываем результат
} AccessState;

static AccessState state;
static char inputBuffer[4];
static uint8_t inputCount;
static uint32_t resultShownAt;

// ---- Вспомогательные функции -----------------------------------------------

// Включает/выключает светодиод на GPIOA через атомарный регистр BSRR:
// запись в биты 0-15 ставит пин в 1, запись в биты 16-31 - сбрасывает в 0.
static void setLed(uint8_t pin, uint8_t on) {
  if (on) {
    GPIOA->BSRR = (1U << pin);
  } else {
    GPIOA->BSRR = (1U << (pin + 16));
  }
}

// Показывает уже введённые цифры кода, остальные разряды - пустые.
static void showInputBuffer(void) {
  uint8_t codes[4] = {0x00, 0x00, 0x00, 0x00};

  for (uint8_t i = 0; i < inputCount; i++) {
    codes[i] = tm1637_digit_code((uint8_t)(inputBuffer[i] - '0'));
  }

  tm1637_display_raw(codes);
}

// Выводит на дисплей один результат проверки: "1" - код верный,
// "0" - код неверный. Остальные 3 разряда остаются пустыми.
static void showResult(uint8_t granted) {
  uint8_t codes[4] = {0x00, 0x00, 0x00, 0x00};
  codes[3] = tm1637_digit_code(granted ? 1 : 0); // крайний правый разряд

  tm1637_display_raw(codes);
}

static void resetInput(void) {
  inputCount = 0;
  state = ACCESS_STATE_INPUT;
  setLed(LED_GRANTED_PIN, 0);
  setLed(LED_DENIED_PIN, 0);
  showInputBuffer();
}

// Проверяет то, что успели ввести (inputCount символов, от 1 до 4).
// Совпадением считается только точное совпадение и длины, и самих цифр -
// код короче или длиннее ACCESS_CODE_LEN заведомо неверный.
static void checkCode(void) {
  uint8_t match = (inputCount == ACCESS_CODE_LEN);

  for (uint8_t i = 0; match && i < inputCount; i++) {
    if (inputBuffer[i] != ACCESS_CODE[i]) {
      match = 0;
    }
  }

  if (match) {
    setLed(LED_GRANTED_PIN, 1);
    setLed(LED_DENIED_PIN, 0);
    showResult(1);
    printf("Access granted\n");
  } else {
    setLed(LED_GRANTED_PIN, 0);
    setLed(LED_DENIED_PIN, 1);
    showResult(0);
    printf("Access denied\n");
  }

  state = ACCESS_STATE_RESULT;
  resultShownAt = tickCount;
}

// ---- Публичный интерфейс ----------------------------------------------------

void initAccess(void) {
  // Тактирование GPIOA уже включено в initGPIO() (RCC_AHBENR_GPIOAEN).
  // Настраиваем PA9 и PA15 как обычные выходы push-pull.
  GPIOA->MODER = (GPIOA->MODER & ~(3U << (LED_GRANTED_PIN * 2))) | (1U << (LED_GRANTED_PIN * 2));
  GPIOA->MODER = (GPIOA->MODER & ~(3U << (LED_DENIED_PIN * 2))) | (1U << (LED_DENIED_PIN * 2));
  GPIOA->OTYPER &= ~(1U << LED_GRANTED_PIN);
  GPIOA->OTYPER &= ~(1U << LED_DENIED_PIN);

  state = ACCESS_STATE_INPUT;
  inputCount = 0;

  setLed(LED_GRANTED_PIN, 0);
  setLed(LED_DENIED_PIN, 0);
  showInputBuffer();
}

void updateAccess(char key) {
  // В состоянии показа результата ждём либо тайм-аут, либо "*" для сброса
  if (state == ACCESS_STATE_RESULT) {
    if (key == '*' || (tickCount - resultShownAt) >= RESULT_SHOW_TIME_MS) {
      resetInput();
    }
    return;
  }

  if (key == '\0') {
    return; // новых нажатий не было
  }

  if (key == '*') {
    resetInput();
    return;
  }

  if (key == '#') {
    // Отправить можно код любой длины от 1 до 4 цифр - "#" просто
    // запускает проверку того, что уже набрано. Пустой ввод игнорируется.
    if (inputCount > 0) {
      checkCode();
    }
    return;
  }

  if (key >= '0' && key <= '9') {
    if (inputCount < 4) {
      inputBuffer[inputCount] = key;
      inputCount++;
      showInputBuffer();
    }
    return;
  }
}
