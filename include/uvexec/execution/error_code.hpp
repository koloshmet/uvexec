/*
 * Copyright (c) 2024 Michael Guzov
 *
 * Licensed under the Apache License Version 2.0 with LLVM Exceptions
 * (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 *   https://llvm.org/LICENSE.txt
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <uv.h>
#include <system_error>


namespace NUvExec {

enum class EErrc {
    ai_address_family_not_supported = UV_EAI_ADDRFAMILY,
    ai_bad_flags = UV_EAI_BADFLAGS,
    ai_bad_node = UV_EAI_NONAME,
    ai_buffer_too_small = UV_EAI_OVERFLOW,
    ai_invalid_hint = UV_EAI_BADHINTS,
    ai_failure = UV_EAI_FAIL,
    ai_family_not_supported = UV_EAI_FAMILY,
    ai_not_enough_memory = UV_EAI_MEMORY,
    ai_no_address = UV_EAI_NODATA,
    ai_protocol_error = UV_EAI_PROTOCOL,
    ai_request_cancelled = UV_EAI_CANCELED,
    ai_service_not_avilable = UV_EAI_SERVICE,
    ai_socket_type_not_supported = UV_EAI_SOCKTYPE,
    ai_temporary_failure_try_again = UV_EAI_AGAIN,
    address_family_not_supported = UV_EAFNOSUPPORT,
    address_in_use = UV_EADDRINUSE,
    address_not_available = UV_EADDRNOTAVAIL,
    already_connected = UV_EISCONN,
    argument_list_too_long = UV_E2BIG,
    bad_address = UV_EFAULT,
    bad_file_descriptor = UV_EBADF,
    broken_pipe = UV_EPIPE,
    connection_aborted = UV_ECONNABORTED,
    connection_already_in_progress = UV_EALREADY,
    connection_refused = UV_ECONNREFUSED,
    connection_reset = UV_ECONNRESET,
    cross_device_link = UV_EXDEV,
    destination_address_required = UV_EDESTADDRREQ,
    device_or_resource_busy = UV_EBUSY,
    directory_not_empty = UV_ENOTEMPTY,
    end_of_file = UV_EOF,
    file_exists = UV_EEXIST,
    file_too_large = UV_EFBIG,
    filename_too_long = UV_ENAMETOOLONG,
    function_not_supported = UV_ENOSYS,
    host_unreachable = UV_EHOSTUNREACH,
    illegal_byte_sequence = UV_EILSEQ,
    inappropriate_file_type = UV_EFTYPE,
    inappropriate_io_control_operation = UV_ENOTTY,
    interrupted = UV_EINTR,
    invalid_argument = UV_EINVAL,
    invalid_character = UV_ECHARSET,
    invalid_seek = UV_ESPIPE,
    io_error = UV_EIO,
    is_a_directory = UV_EISDIR,
    message_size = UV_EMSGSIZE,
    network_down = UV_ENETDOWN,
    network_unreachable = UV_ENETUNREACH,
    no_buffer_space = UV_ENOBUFS,
    no_message_available = UV_ENODATA,
    no_protocol_option = UV_ENOPROTOOPT,
    no_space_on_device = UV_ENOSPC,
    no_such_device_or_address = UV_ENXIO,
    no_such_device = UV_ENODEV,
    no_such_file_or_directory = UV_ENOENT,
    no_such_process = UV_ESRCH,
    not_a_directory = UV_ENOTDIR,
    not_a_socket = UV_ENOTSOCK,
    not_connected = UV_ENOTCONN,
    not_enough_memory = UV_ENOMEM,
    not_on_the_network = UV_ENONET,
    not_supported = UV_ENOTSUP,
    operation_canceled = UV_ECANCELED,
    operation_not_permitted = UV_EPERM,
    permission_denied = UV_EACCES,
    protocol_error = UV_EPROTO,
    protocol_driver_not_attached = UV_EUNATCH,
    protocol_not_supported = UV_EPROTONOSUPPORT,
    read_only_file_system = UV_EROFS,
    resource_unavailable_try_again = UV_EAGAIN,
    result_out_of_range = UV_ERANGE,
    socket_type_not_supported = UV_ESOCKTNOSUPPORT,
    text_file_busy = UV_ETXTBSY,
    timed_out = UV_ETIMEDOUT,
    too_many_files_open_in_system = UV_ENFILE,
    too_many_files_open = UV_EMFILE,
    too_many_links = UV_EMLINK,
    too_many_symbolic_link_levels = UV_ELOOP,
    transport_endpoint_shutdown = UV_ESHUTDOWN,
    unknown_error = UV_UNKNOWN,
    value_too_large = UV_EOVERFLOW,
    wrong_protocol_type = UV_EPROTOTYPE
};

auto Category() noexcept -> const std::error_category&;

auto make_error_code(EErrc e) noexcept -> std::error_code;

};

template<>
struct std::is_error_code_enum<NUvExec::EErrc> : std::true_type {};
