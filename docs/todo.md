**1. Loại bỏ Dynamic Global Lookup (Dễ)**

  * **Hiện tại:** `GET_GLOBAL` nhận một `name_idx` (chuỗi), VM phải hash chuỗi đó và tìm trong hash map global.
  * **Cải tiến:** Compiler phải giải quyết tất cả biến Global thành một mảng chỉ số (index).
      * VM sẽ có `std::vector<Value> globals_`.
      * OpCode đổi thành: `GET_GLOBAL <index>`. Truy cập mảng `globals_[index]` cực nhanh (O(1)) so với hash map.
      * **Compiler:** Phải xây dựng bảng symbol cho global scope.

**2. "Specialized Bytecode" thay vì Polymorphic Opcodes (Trung bình)**

  * **Hiện tại:** `OpCode::ADD` trong `MeowVM::run` gọi `op_dispatcher_->find(...)`. Nó phải kiểm tra kiểu của toán hạng (Int? Float? String?) *mỗi lần chạy*.
  * **Cải tiến:** Nếu Compiler biết kiểu dữ liệu (thông qua Type Checking hoặc Type Inference), hãy phát sinh opcode cụ thể:
      * `ADD_INT`: Cộng 2 thanh ghi int (không check kiểu, hoặc chỉ assert).
      * `ADD_FLOAT`: Cộng 2 thanh ghi float.
      * `CONCAT_STRING`: Nối chuỗi.
  * **VM:** Sẽ không cần `OperatorDispatcher` phức tạp nữa, chỉ cần thực thi trực tiếp phép `+`. Nếu sai kiểu -\> Crash hoặc Undefined Behavior (như C++), ép người viết code MeowScript phải đúng kiểu.

**3. Inline Caching cho Property Access (Khó - High Performance)**

  * **Hiện tại:** `GET_PROP` tìm chuỗi tên thuộc tính trong Hash Map của Instance/Class.
  * **Cải tiến:** Vì VM "không được thông minh", ta dùng kỹ thuật **Inline Cache**.
      * Opcode `GET_PROP` sẽ có thêm 1 slot trống trong bytecode để "nhớ" vị trí (offset) của thuộc tính lần truy cập trước.
      * Lần chạy đầu: Tìm Hash Map, lưu offset vào slot đó.
      * Lần chạy sau: Kiểm tra nhanh (nếu class không đổi), dùng offset đó truy cập thẳng vào mảng field, bỏ qua Hash Map.



### 1\. Giai đoạn Chuẩn bị: Interface & Định nghĩa

#### 📂 `include/vm/meow_engine.h`

```cpp
#pragma once

namespace meow::memory { class MemoryManager; }

namespace meow::vm {

class MeowEngine {
public:
    virtual ~MeowEngine() = default;
    
    virtual meow::memory::MemoryManager* get_heap() = 0; 
};

} // namespace meow::vm
```

#### 📂 `include/core/definitions.h`

```cpp
#pragma once
#include "common/pch.h"

namespace meow::vm { class MeowEngine; }

namespace meow::core {
    class Value;
    
    using native_fn_t = Value (*)(meow::vm::MeowEngine* engine, Value* args, int argc);
    
    // ... (các using khác giữ nguyên) ...
}
```

-----

### 2\. Giai đoạn Cốt lõi: Value & Stack Frame

#### 📂 `include/core/value.h`

```cpp
// ...
// [Fix] NativeFn -> native_fn_t
using base_t = meow::variant<null_t, bool_t, int_t, float_t, object_t, native_fn_t>;

class Value {
    // ... 

    inline Value(native_fn_t fn) noexcept : data_(fn) {}

    [[nodiscard]] inline bool is_native_fn() const noexcept { 
        return data_.holds<native_fn_t>(); 
    }
    
    [[nodiscard]] inline native_fn_t as_native_fn() const noexcept { 
        return data_.get<native_fn_t>(); 
    }
    
    // ...
};
```

#### 📂 `include/runtime/execution_context.h`

```cpp
namespace meow::runtime {

struct CallFrame {
    meow::core::function_t function_;
    meow::core::module_t module_;
    
    meow::core::Value* slots_; 
    
    size_t ret_reg_; 
    const uint8_t* ip_;

    CallFrame(meow::core::function_t function, meow::core::module_t module, 
              meow::core::Value* slots, 
              size_t ret_reg, const uint8_t* ip)
        : function_(function), module_(module), slots_(slots), ret_reg_(ret_reg), ip_(ip) {
    }
};
// ...
}
```

-----

### 3\. Giai đoạn Kiến trúc: MeowVM

#### 📂 `include/vm/meow_vm.h`

```cpp
// ...
namespace meow::vm {

static constexpr size_t STACK_MAX = 1024 * 256;
static constexpr size_t FRAMES_MAX = 1024 * 4; 

class MeowVM : public MeowEngine {
public:
    // ...
    
    // Implement MeowEngine
    meow::memory::MemoryManager* get_heap() override {
        return heap_.get();
    }

private:
    meow::core::Value stack_[STACK_MAX]; 
    meow::core::Value* stack_top_; 

    meow::runtime::CallFrame frames_[FRAMES_MAX];
    size_t frame_count_ = 0;

    // ...
};
}
```

-----

### 4\. Giai đoạn Thực thi: Logic (`src/vm/meow_vm.cpp`)

#### a. Constructor & Macro

```cpp
// Constructor
MeowVM::MeowVM(...) {
    // ... 
    
    stack_top_ = stack_; 
}

// Macro REGISTER
#undef REGISTER 
#define REGISTER(idx) (context_->current_frame_->slots_[idx]) 
```

#### b. Logic `prepare()`

```cpp
void MeowVM::prepare() noexcept {
    // ... (tạo main_func, main_module) ...

    stack_top_ = stack_;
    frame_count_ = 0;

    if (frame_count_ < FRAMES_MAX) {
        Value* base = stack_top_;
        
        size_t num_regs = main_proto->get_num_registers();
        stack_top_ += num_regs; 

        frames_[frame_count_++] = CallFrame(
            main_func, main_module, 
            base, 
            -1, 
            main_func->get_proto()->get_chunk().get_code()
        );
        
        context_->current_frame_ = &frames_[0];
    }
}
```

#### c. `op_CALL`

```cpp
op_CALL: {
    uint16_t dst_idx = READ_U16();
    uint16_t fn_reg = READ_U16();
    uint16_t arg_start = READ_U16();
    uint16_t argc = READ_U16();

    Value callee = REGISTER(fn_reg);

    if (callee.is_native_fn()) {
        native_fn_t fn = callee.as_native_fn();
        
        Value* args = &REGISTER(arg_start);
        Value result = fn(this, args, argc);
        
        if (dst_idx != 0xFFFF) {
            REGISTER(dst_idx) = result;
        }
        DISPATCH();
    }

    else if (callee.is_function()) {
        function_t closure = callee.as_function();
        proto_t proto = closure->get_proto();
        
        if (frame_count_ == FRAMES_MAX) throw_vm_error("Stack overflow!");
        
        Value* new_base = &REGISTER(arg_start);
        
        if (new_base + proto->get_num_registers() >= stack_ + STACK_MAX) {
             throw_vm_error("Stack overflow (registers)!");
        }

        CallFrame* frame = &frames_[frame_count_++];
        *frame = CallFrame(
            closure, 
            context_->current_frame_->module_, 
            new_base,                           
            dst_idx,                           
            proto->get_chunk().get_code()
        );

        Value* needed_top = new_base + proto->get_num_registers();
        if (needed_top > stack_top_) stack_top_ = needed_top;

        context_->current_frame_ = frame;
        ip = frame->ip_;
        
        DISPATCH();
    }
    // ...
}
```

#### d. `op_RETURN`

```cpp
op_RETURN: {
    uint16_t ret_reg_idx = READ_U16();
    Value return_value = (ret_reg_idx == 0xFFFF) ? Value(null_t{}) : REGISTER(ret_reg_idx);
    
    size_t ret_dst_idx = context_->current_frame_->ret_reg_; 
    
    frame_count_--;
    
    if (frame_count_ == 0) {
        return;
    }

    context_->current_frame_ = &frames_[frame_count_ - 1];
    ip = context_->current_frame_->ip_;
    
    if (ret_dst_idx != static_cast<size_t>(-1)) {
        REGISTER(ret_dst_idx) = return_value;
    }
    
    DISPATCH();
}
```

-----

### 5\. Dọn dẹp tàn dư

  * **Xóa:** `include/core/objects/native.h`.
  * **Sửa:** `include/core/meow_object.h` -\> Xóa `ObjectType::NATIVE_FN`.
  * **Sửa:** `include/core/object_traits.h` -\> Xóa traits `ObjNativeFunction`.
  * **Sửa:** `src/memory/memory_manager.cpp` -\> Xóa `new_native`.

**Mục tiêu:** Hoàn thiện hệ thống báo lỗi cho V2 bằng cách:

1.  Tích hợp module `diagnostics` vào `Machine`.
2.  Tái hiện khả năng "Stack Trace" (truy vết ngăn xếp) từ Legacy nhưng đẹp hơn.
3.  Thêm khả năng hiển thị ngữ cảnh (Context) khi lỗi xảy ra.

Dưới đây là mã nguồn để hoàn thiện hệ thống báo lỗi:

### 1\. Cập nhật `VMError` để chứa thông tin cấu trúc

Thay vì chỉ chứa chuỗi, `VMError` cần chứa `Diagnostic` để `main.cpp` có thể render đẹp mắt.

**File:** `include/vm/machine.h` (Sửa lại struct VMError)

```cpp
#include "diagnostics/diagnostic.h" // Thêm include này

namespace meow::inline vm {

// VMError bây giờ chứa một object Diagnostic đầy đủ
struct VMError : public std::runtime_error {
    meow::diagnostics::Diagnostic diag;

    explicit VMError(meow::diagnostics::Diagnostic d) 
        : std::runtime_error(d.code + ": " + (d.args.count("msg") ? d.args.at("msg") : "")), 
          diag(std::move(d)) {}
};

// ... (Phần còn lại của class Machine giữ nguyên)
```

### 2\. Triển khai logic tạo báo lỗi trong `Machine`

Chúng ta cần viết một hàm helper `create_runtime_error` trong `Machine` để thu thập Call Stack và tạo object `Diagnostic`.

**File:** `src/vm/machine.cpp`

Thêm đoạn code này vào phần implement của `Machine`, hoặc tách ra file riêng nếu muốn gọn. Ở đây mình sẽ cài đè vào logic `throw_vm_error` cũ.

```cpp
// Helper: Lấy dòng hiện tại (Giả sử Chunk có hỗ trợ debug lines, nếu chưa có thì trả về 0)
// Bạn cần bổ sung vector lines vào Chunk sau này để có số dòng chính xác.
size_t get_current_line(const Chunk& chunk, size_t ip_offset) {
    // TODO: Implement getLine(ip_offset) trong Chunk class
    return 0; 
}

// Hàm helper nội bộ để xây dựng Diagnostic từ trạng thái VM hiện tại
meow::diagnostics::Diagnostic Machine::create_runtime_error(const std::string& message) {
    using namespace meow::diagnostics;

    Diagnostic d;
    d.code = "RUNTIME_ERROR";
    d.severity = Severity::Error;
    d.args["msg"] = message;

    // 1. Xây dựng Stack Trace (Call Stack)
    // Duyệt ngược từ frame hiện tại về main
    for (auto it = context_->call_stack_.rbegin(); it != context_->call_stack_.rend(); ++it) {
        const CallFrame& frame = *it;
        
        CallFrameInfo info; // (Lưu ý: diagnostics::CallFrame đổi tên thành CallFrameInfo để tránh trùng với runtime::CallFrame nếu bị conflict namespace)
        // Hoặc dùng meow::diagnostics::CallFrame trực tiếp:
        meow::diagnostics::CallFrame diagFrame;
        
        // Lấy tên hàm
        if (frame.function_ && frame.function_->get_proto()) {
            auto name = frame.function_->get_proto()->get_name();
            diagFrame.function = name ? std::string(name->c_str()) : "<anonymous>";
        } else {
            diagFrame.function = "<script>";
        }

        // Lấy file path
        if (frame.module_) {
            auto path = frame.module_->get_file_path();
            diagFrame.file = path ? std::string(path->c_str()) : "";
        }

        // Tính toán IP offset và dòng
        if (frame.function_ && frame.function_->get_proto()) {
            const Chunk& chunk = frame.function_->get_proto()->get_chunk();
            size_t offset = frame.ip_ - chunk.get_code();
            // Giảm 1 vì IP trỏ đến lệnh tiếp theo
            if (offset > 0) offset--; 
            
            // diagFrame.line = chunk.getLine(offset); // Cần implement trong Chunk
            diagFrame.line = 0; // Placeholder
            diagFrame.col = 0;
        }

        d.callstack.push_back(diagFrame);
    }

    // 2. Thêm Snapshot (Tùy chọn: Tương tự Legacy nhưng gọn hơn)
    // Có thể thêm vào d.notes nếu muốn hiển thị giá trị register hiện tại
    return d;
}

// Sửa lại hàm throw_vm_error trong Machine class
[[noreturn]] void Machine::throw_vm_error(const std::string& message) {
    auto diag = create_runtime_error(message);
    throw VMError(std::move(diag));
}
```

### 3\. Cập nhật `Machine::run` để bắt lỗi

Sửa lại khối `catch` trong `run()` để hiển thị lỗi đẹp hơn.

**File:** `src/vm/machine.cpp`

```cpp
void Machine::run() {
    // ... (phần dispatch loop giữ nguyên)

    } catch (const VMError& e) {
        // In lỗi ra màn hình sử dụng hệ thống Diagnostics xịn của V2
        meow::diagnostics::SimpleLocaleSource locale; // Có thể load từ file sau
        meow::diagnostics::RenderOptions opts;
        opts.enable_color = true;
        
        // Render lỗi ra stderr
        std::string formatted_error = meow::diagnostics::render_to_human(e.diag, locale, opts);
        std::cerr << formatted_error << std::endl;

        // Logic Exception Handling (Try/Catch trong ngôn ngữ Meow)
        if (context_->exception_handlers_.empty()) {
            // Không có try/catch nào bắt -> Chết chương trình
            return; 
        }

        // ... (Phần logic stack unwinding giữ nguyên như cũ) ...
        // Lưu ý: Bạn cần chuyển message từ e.diag.args["msg"] vào register lỗi
        if (context_->current_base_ < context_->registers_.size()) {
             // Lấy message gốc
            REGISTER(0) = Value(heap_->new_string(e.diag.args["msg"]));
        }
        goto dispatch_start;
    }
}
```

### 4\. Bổ sung Debug Lines cho `Chunk` (Khuyên dùng)

Để báo lỗi chính xác "Error at line 10", bạn cần thêm thông tin dòng vào `Chunk`.

**File:** `include/runtime/chunk.h`

```cpp
class Chunk {
private:
    // Run-length encoding cho số dòng để tiết kiệm bộ nhớ
    // Ví dụ: <line_number, count>
    std::vector<std::pair<int, int>> lines_; 

public:
    // ...
    
    void add_line(int line) {
        if (!lines_.empty() && lines_.back().first == line) {
            lines_.back().second++;
        } else {
            lines_.emplace_back(line, 1);
        }
    }

    [[nodiscard]] int get_line(size_t offset) const noexcept {
        size_t current_offset = 0;
        for (const auto& [line, count] : lines_) {
            current_offset += count;
            if (offset < current_offset) return line;
        }
        return -1;
    }
};
```