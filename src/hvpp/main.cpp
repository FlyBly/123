#include "lib/driver.h"
#include "lib/assert.h"
#include "lib/log.h"
#include "lib/mm.h"
#include "lib/mp.h"

#include "hvpp/hypervisor.h"
#include "hvpp/vmexit_compositor.h"

#include "vmexit_custom.h"
#include "device_custom.h"

#include <cinttypes>
   
using namespace ia32;
using namespace hvpp;

namespace driver
{
  //
  // Create combined handler from these VM-exit handlers.
  //
  using vmexit_handler_t = vmexit_custom_handler;
  
  /*
  vmexit_compositor_handler<
    vmexit_stats_handler,
    // vmexit_dbgbreak_handler,
    vmexit_custom_handler
    >;
    */

  static_assert(std::is_base_of_v<vmexit_handler, vmexit_handler_t>);

  hypervisor*       hypervisor_      = nullptr;
  vmexit_handler_t* vmexit_handler_  = nullptr;
  device_custom*    device_          = nullptr;

  auto initialize() noexcept -> error_code_t
  {
    //
    // Create device instance.
    //
    device_ = new device_custom();

    if (!device_)
    {
      destroy();
      return make_error_code_t(std::errc::not_enough_memory);
    }

    //
    // Initialize device instance.
    //
    if (auto err = device_->initialize())
    {
      destroy();
      return err;
    }

    //
    // Create hypervisor instance.
    //
    hypervisor_ = new hypervisor();

    if (!hypervisor_)
    {
      destroy();
      return make_error_code_t(std::errc::not_enough_memory);
    }

    //
    // Initialize hypervisor.
    //
    if (auto err = hypervisor_->initialize())
    {
      destroy();
      return err;
    }

    //
    // Create VM-exit handler instance.
    //
    vmexit_handler_ = new vmexit_handler_t();

    if (!vmexit_handler_)
    {
      destroy();
      return make_error_code_t(std::errc::not_enough_memory);
    }

    //
    // Initialize VM-exit handler.
    //
    if (auto err = vmexit_handler_->initialize())
    {
      destroy();
      return err;
    }

    //
    // Assign the vmexit_dbgbreak_handler instance to the device.
    //
    //device_->handler(std::get<vmexit_dbgbreak_handler>(vmexit_handler_->handlers));

    //
    // Example: Enable tracing of I/O instructions.
    //
    /*
    std::get<vmexit_stats_handler>(vmexit_handler_->handlers)
      .trace_bitmap().set(int(vmx::exit_reason::execute_io_instruction));
    */
    //
    // Start the hypervisor.
    //
    hypervisor_->start(vmexit_handler_);

    //
    // Tell debugger we're started.
    //
    hvpp_info("Hypervisor started, current free memory: %" PRIu64 " MB",
               memory_manager::free_bytes() / 1024 / 1024);

    return error_code_t{};
  }

  void destroy() noexcept
  {
    //
    // Stop and destroy hypervisor.
    //
    if (hypervisor_)
    {
      hypervisor_->stop();
      hypervisor_->destroy();
      delete hypervisor_;
    }

    //
    // Destroy VM-exit handler.
    //
    if (vmexit_handler_)
    {
      //
      // Print statistics into debugger.
      //
      /*
      std::get<vmexit_stats_handler>(vmexit_handler_->handlers).dump();
      */
      vmexit_handler_->destroy();
      delete vmexit_handler_;
    }

    //
    // Destroy device.
    //
    if (device_)
    {
      device_->destroy();
      delete device_;
    }

    hvpp_info("Hypervisor stopped");
  }
}
