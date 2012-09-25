#include <vector>
#include <iostream>

enum MessageType {
	MSG_UNKNOWN = 0,
	MSG_KEY,
	MSG_MOUSE
};

class Message {
public:
	MessageType m_Type;
	int m_Id;
	Message(MessageType type, int id) : m_Type(type), m_Id(id) {}
};

class KeyMessage : public Message {
public:
	KeyMessage(int id) : Message(MSG_KEY, id) {}
};

class MouseMessage : public Message {
public:
	MouseMessage(int id) : Message(MSG_MOUSE, id) {}
};

class MessagingBase {
	typedef void(*Handler)(MessagingBase*, const Message&);

	struct Binding {
		MessageType m_Type;
		MessagingBase* m_Observer;
		MessagingBase* m_Observed;
		Handler m_Handler;

		Binding(MessageType type, MessagingBase* observer,
			MessagingBase* observed, Handler handler) :
		m_Type(type), m_Observer(observer), m_Observed(observed),
			m_Handler(handler) {}
	};

	std::vector<Binding> m_Bindings;

public:
	~MessagingBase();

	template <typename Object, typename Param, void(Object::*Method)(const Param& param)>
	void Bind(MessageType type, Object* observer);
	template <typename Object, typename Param, void(Object::*Method)(const Param& param)>
	void Unbind(MessageType type, Object* observer);
	void Unbind(const MessagingBase* object);

protected:
	void SendMessage(const Message& msg);

private:
	template <typename Object, typename Param, void(Object::*Method)(Param param)>
	static void Binder(MessagingBase* observer, const Message& message) {
		(static_cast<Object*>(observer)->*Method)(static_cast<const Param&>(message));
	}
};

class Observed : public MessagingBase {
public:
	void RaiseKey(int id) { SendMessage(KeyMessage(id)); }
	void RaiseMouse(int id) { SendMessage(MouseMessage(id)); }
};

class Observer : public MessagingBase {
public:
	void OnMessage(const Message& msg) { std::cout << "Got message id " << msg.m_Id << " of type " << msg.m_Type << std::endl; }
	void OnKey(const KeyMessage& msg) { std::cout << "Got key message id " << msg.m_Id << std::endl; }
	void OnMouse(const MouseMessage& msg) { std::cout << "Got mouse message id " << msg.m_Id << std::endl; }
};

int main(int argc, char** argv)
{
	Observed observed;
	Observer observer;

	observed.Bind<Observer, Message, &Observer::OnMessage>(MSG_UNKNOWN, &observer);
	observed.Bind<Observer, MouseMessage, &Observer::OnMouse>(MSG_MOUSE, &observer);
	observed.Bind<Observer, KeyMessage, &Observer::OnKey>(MSG_KEY, &observer);

	std::cout << "Sending key message id 1" << std::endl;
	observed.RaiseKey(1);

	std::cout << "Sending mouse message id 2" << std::endl;
	observed.RaiseMouse(2);

	observed.Unbind<Observer, MouseMessage, &Observer::OnMouse>(MSG_MOUSE, &observer);

	std::cout << "Sending mouse message id 3" << std::endl;
	observed.RaiseMouse(3);

	observed.Unbind(&observer);

	std::cout << "Sending key message id 4" << std::endl;
	observed.RaiseKey(4);

	std::cout << "Done" << std::endl;
	std::getc(stdin);
}

MessagingBase::~MessagingBase() {
	while (!m_Bindings.empty()) {
		Binding binding = m_Bindings.back();
		m_Bindings.pop_back();
		binding.m_Observed->Unbind(this);
		binding.m_Observer->Unbind(this);
	};
}

void MessagingBase::SendMessage(const Message& msg) {
	for (size_t i = 0; i < m_Bindings.size(); ++i)
		if (m_Bindings[i].m_Type == MSG_UNKNOWN || m_Bindings[i].m_Type == msg.m_Type)
			m_Bindings[i].m_Handler(m_Bindings[i].m_Observer, msg);
}

template <typename Object, typename Param, void(Object::*Method)(const Param& param)>
void MessagingBase::Bind(MessageType type, Object* observer) {
	Binding binding(type, observer, this, &Binder<Object, const Param&, Method>);
	m_Bindings.push_back(binding);
	observer->m_Bindings.push_back(binding);
}

template <typename Object, typename Param, void(Object::*Method)(const Param& param)>
void MessagingBase::Unbind(MessageType type, Object* observer) {
	Handler handler = &Binder<Object, const Param&, Method>;
	auto i = m_Bindings.begin();
	while (i != m_Bindings.end())
		if (i->m_Type == type && i->m_Observer == observer && i->m_Handler == handler)
			i = m_Bindings.erase(i);
		else
			++i;
}

void MessagingBase::Unbind(const MessagingBase* object) {
	auto i = m_Bindings.begin();
	while (i != m_Bindings.end())
		if (i->m_Observed == object || i->m_Observer == object)
			i = m_Bindings.erase(i);
		else
			++i;
}