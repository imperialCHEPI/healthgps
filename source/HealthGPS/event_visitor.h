#pragma once
namespace hgps {

	struct RunnerEventMessage;
	struct InfoEventMessage;
	struct ErrorEventMessage;
	struct ResultEventMessage;

	class EventMessageVisitor {
	public:
		EventMessageVisitor() = default;

		EventMessageVisitor(const EventMessageVisitor&) = delete;
		EventMessageVisitor& operator=(const EventMessageVisitor&) = delete;

		EventMessageVisitor(EventMessageVisitor&&) = delete;
		EventMessageVisitor& operator=(EventMessageVisitor&&) = delete;

		virtual ~EventMessageVisitor() {};

		virtual void visit(const RunnerEventMessage& message) = 0;

		virtual void visit(const InfoEventMessage& message) = 0;

		virtual void visit(const ErrorEventMessage& message) = 0;

		virtual void visit(const ResultEventMessage& message) = 0;
	};
}