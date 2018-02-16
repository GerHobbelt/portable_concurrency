#pragma once

#include <future>

#include "future.h"
#include "future_state.h"
#include "shared_future.h"
#include "then.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {

template<typename T>
shared_future<T>::shared_future(future<T>&& rhs) noexcept:
    state_(std::move(rhs.state_))
{}

template<typename T>
void shared_future<T>::wait() const {
  if (!state_)
    throw std::future_error(std::future_errc::no_state);
  auto& continuations = state_->continuations();
  if (!continuations.executed())
    continuations.get_waiter().wait();
}

template<typename T>
template<typename Rep, typename Period>
future_status shared_future<T>::wait_for(const std::chrono::duration<Rep, Period>& rel_time) const {
  if (!state_)
    throw std::future_error(std::future_errc::no_state);
  auto& continuations = state_->continuations();
  if (continuations.executed())
    return future_status::ready;
  return continuations.get_waiter().wait_for(std::chrono::duration_cast<std::chrono::nanoseconds>(rel_time)) ?
    future_status::ready:
    future_status::timeout
  ;
}

template<typename T>
template <typename Clock, typename Duration>
future_status shared_future<T>::wait_until(const std::chrono::time_point<Clock, Duration>& abs_time) const {
  return wait_for(abs_time - Clock::now());
}

template<typename T>
bool shared_future<T>::valid() const noexcept {return static_cast<bool>(state_);}

template<typename T>
typename shared_future<T>::get_result_type shared_future<T>::get() const {
  if (!state_)
    throw std::future_error(std::future_errc::no_state);
  wait();
  return state_->value_ref();
}

template<typename T>
bool shared_future<T>::is_ready() const {
  if (!state_)
    throw std::future_error(std::future_errc::no_state);
  return state_->continuations().executed();
}

template<typename T>
template<typename F>
detail::cnt_future_t<F, shared_future<T>> shared_future<T>::then(F&& f) const {
  return then(detail::inplace_executor{}, std::forward<F>(f));
}

template<typename T>
template<typename E, typename F>
auto shared_future<T>::then(E&& exec, F&& f) const -> std::enable_if_t<
  is_executor<std::decay_t<E>>::value,
  detail::cnt_future_t<F, shared_future<T>>
> {
  if (!state_)
    throw std::future_error(std::future_errc::no_state);
  return detail::make_then_state<detail::cnt_tag::shared_then, T, E, F>(
    std::move(state_), std::forward<E>(exec), std::forward<F>(f)
  );
}

template<typename T>
template<typename F>
detail::cnt_future_t<F, typename shared_future<T>::get_result_type> shared_future<T>::next(F&& f) const {
  return next(detail::inplace_executor{}, std::forward<F>(f));
}

template<typename T>
template<typename E, typename F>
auto shared_future<T>::next(E&& exec, F&& f) const -> std::enable_if_t<
  is_executor<std::decay_t<E>>::value,
  detail::cnt_future_t<F, get_result_type>
> {
  if (!state_)
    throw std::future_error(std::future_errc::no_state);
  return detail::make_then_state<detail::cnt_tag::shared_next, T, E, F>(
    state_, std::forward<E>(exec), std::forward<F>(f)
  );
}

template<typename T>
void shared_future<T>::detach() {
  if (!state_)
    throw std::future_error(std::future_errc::no_state);
  auto stateref = state_;
  stateref->push([captured_state = std::move(state_)] {});
}

template<typename T>
shared_future<T>::shared_future(std::shared_ptr<detail::future_state<T>>&& state) noexcept:
  state_(std::move(state))
{}

#if defined(__cpp_coroutines)
template<typename T>
bool shared_future<T>::await_ready() const noexcept {return is_ready();}

template<typename T>
typename shared_future<T>::get_result_type shared_future<T>::await_resume() const {return get();}

template<typename T>
void shared_future<T>::await_suspend(std::experimental::coroutine_handle<> handle) const {
  state_->push_continuation(std::move(handle));
}
#endif

namespace detail {

template<typename T>
std::shared_ptr<future_state<T>>& state_of(shared_future<T>& f) {
  return f.state_;
}

} // namespace detail

} // inline namespace cxx14_v1
} // namespace portable_concurrency
