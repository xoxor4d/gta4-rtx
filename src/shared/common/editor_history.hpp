#pragma once
#include <vector>

namespace shared::common
{
	// undo / redo of an editor through snapshots of everything an edit can change
	template <typename snapshot_t>
	class editor_history
	{
	public:
		/// once per frame before any edit: 'interacting' is true while a widget or gizmo is held, 'take' returns the current state
		template <typename take_fn>
		void begin_frame(const bool interacting, take_fn&& take)
		{
			if (!interacting) {
				m_in_edit = false;
			}

			if (!m_in_edit) {
				m_frame_snapshot = take();
			}
		}

		/// call after every change: the first change of an interaction puts the state before it on the undo stack
		void commit()
		{
			if (m_in_edit) {
				return;
			}

			m_undo.emplace_back(m_frame_snapshot);
			if (m_undo.size() > m_limit) {
				m_undo.erase(m_undo.begin());
			}

			m_redo.clear();
			m_in_edit = true;
		}

		/// 'apply' makes a snapshot the current state, false if there is nothing to undo
		template <typename take_fn, typename apply_fn>
		bool undo(take_fn&& take, apply_fn&& apply) {
			return restore(m_undo, m_redo, take, apply);
		}

		template <typename take_fn, typename apply_fn>
		bool redo(take_fn&& take, apply_fn&& apply) {
			return restore(m_redo, m_undo, take, apply);
		}

		bool can_undo() const {
			return !m_undo.empty();
		}

		bool can_redo() const {
			return !m_redo.empty();
		}

		void clear()
		{
			m_undo.clear();
			m_redo.clear();
			m_in_edit = false;
		}

		void set_limit(const std::size_t limit) {
			m_limit = limit;
		}

	private:
		template <typename take_fn, typename apply_fn>
		bool restore(std::vector<snapshot_t>& from, std::vector<snapshot_t>& to, take_fn& take, apply_fn& apply)
		{
			if (from.empty()) {
				return false;
			}

			to.emplace_back(take());
			apply(std::move(from.back()));
			from.pop_back();

			m_frame_snapshot = take();
			m_in_edit = false;
			return true;
		}

		std::vector<snapshot_t> m_undo;
		std::vector<snapshot_t> m_redo;
		snapshot_t m_frame_snapshot;
		std::size_t m_limit = 64u;
		bool m_in_edit = false;
	};
}
