#ifndef _adevs_h_
#define _adevs_h_

/**
\mainpage
<P>This new version of the <I>adevs</I> (A Discrete EVent
system Simulator) library supports the construction of discrete event models using
a simple, messaging passing type API and the Parallel DEVS formalism. Ad hoc
support is offered for dynamic structure models via methods for adding and removing
models from the simulation kernel at runtime.</P>

<P>The message passing API offers many of the advantages of PDEVS: specifically,
its natural resolution of parallel events and the assignment of a unique state to
each instant of (super dense) time. At the same time, it is intuitive to use and
does not require familiarity with the PDEVS formalism.</P>

<P>Support is also provided for the full Parallel DEVS formalism defined on a super
dense time base. This support is based on the message passing API, and so models
constructed using the two methods can be intermingled. Several texts are available
for those who are not already familiar with DEVS in one of its many forms. See
<A HREF="http://www.acims.arizona.edu/">www.acims.arizona.edu</A>
for an extensive collection of tutorials, research papers, and alternate
implementations of the DEVS formalism. Specific books and papers that influenced
the development of this tool are</P>
<UL>
        <LI><P> A. M. Uhrmacher. Dynamic structures in modeling and simulation: a
        reflective approach, <EM>ACM Transactions on Modeling and Computer Simulation, Vol.</EM>
        11, No. 2 , pp. 206-232. April 2001. DOI=10.1145/384169.384173
        <A
HREF="http://doi.acm.org/10.1145/384169.384173">http://doi.acm.org/10.1145/384169.384173</A>. The
approach by <I>adevs</I> to modeling and simulation of dynamic structure systems is based loosely on
this paper.</P></LI>

        <LI><P>Bernard
        P. Zeigler, Tag Gon Kim and Herbert Praehofer. <I>Theory
        of Modeling and Simulation, Second Edition.</I>Academic
        Press. 2000. The publisher's website is
        <A
HREF="http://www.elsevierdirect.com/product.jsp?isbn=9780127784557">http://www.elsevierdirect.com/product.jsp?isbn=9780127784557</A>,
        and this book is available from most on-line booksellers. The
        Discrete Event System Specification (DEVS) is developed in this book
        from its roots in abstract systems theory.</P></LI>

        <LI><P>James
        J. Nutaro. <I>Building
        Software for Simulation: Theory and Algorithms, with Applications in
        C++.</I> Wiley. 2010. The publisher's website is
        <A
HREF="http://www.wiley.com/WileyCDA/WileyTitle/productCd-0470414693.html">http://www.wiley.com/WileyCDA/WileyTitle/productCd-0470414693.html</A>,
        and this book is also available from most on-line booksellers. This
        book presents the Discrete Event Systems Specification along side
        code for a (slightly abridged and older) simulator
        and examples of its use.</P></LI>

        <LI><P>François
        E. Cellier and Ernesto Kofman. <I>Continuous
        System Simulation.</I> Springer. 2006. The publisher's website is
        <A
HREF="http://www.springer.com/us/book/9780387261027">http://www.springer.com/us/book/9780387261027</A>.
        The numerical methods used in the continuous system solvers and the
        discontinuity locking approach that is used for hybrid models are
        described here.
        </P></LI>
</UL>
<P>Question and comments about this software can be sent to its
maintainer, Jim Nutaro, at <A HREF="mailto:nutarojj@ornl.gov">nutarojj@ornl.gov</A>.
</P>
<H2>Building models</H2>
<P>A simulation program is constructed from a set of models, realized as instances of the Model
class. The models are added to a simulation environment (sometimes called a simulation context) that
managements time and communications between the models. The simulation context provides four
services:</P> <UL> <LI><P>Send a message to a model. The message will be delivered at the instant
following the current time.</P> <LI><P>Get the current time.</P> <LI><P>Add a model to the context.
The model comes into existance immediately.</P> <LI><P>Remove a model from the context. The model is
removed at the next instant of simulation time.</P>
</UL>
<P>Each model implements four methods that are invoked by the simulation context as the computation
proceeds, and the context's services may be accessed by the model at these times. These methods
are:</P> <UL> <LI><P>The init() method. This method is called when the model is added to the
simulation context. Its purpose is to set the state of the model at the current time. The context
may be used to send messages to another model, add new models, and remove existing models. The
method returns the next time at which the model will change its state if no messages are received in
the interim. This returned time must be later than the current time.</P></LI> <LI><P>The update()
methods. There are two forms of this method: one that delivers messages sent to the model and the
other that is used when no messages are available. The first version is called at the moment of the
model's next scheduled event when no input messages have arrived prior to that time. In this case,
the current simulation time will be equal to the time returned by the most recent call of init() or
update(). The update() method sets the state of the model for the current time, and then it returns
the next time at which the model will change its state. This time must be in the future. As with
init(), the context may be used to send messages and add or remove models. The second version of
update() is called when messages have been sent to the model. In this case, the current time will be
the instant following when the messages were sent. This time will be less than or equal to the
model's next scheduled event.</P></LI> <P>The model's update() method is called at most once at any
instant of simulation time. Its purpose is to establish the state of the model at the time of the
call. The new state will depend on the previous state and messages sent by other models in the
instant prior to the new state. This model of computation can be mapped directly to the super dense
variant of the Parallel DEVS formalism desecribed in <I>Building software for simulation</I>, and
this mapping is the basis for the Parallel DEVS support that is also offered by the <I>adevs</I>
library.</P></LI> <LI><P>The fini() method. This method is invoked when a model has been removed
from the simulation context. The method is provided with the current time, which is the instant
following the request for it to be removed.</P></LI>
</UL>
<P> The simulation time is described by a pair (t,c). The first element of the pair is the
<em>real</em> time, which corresponds to some physically meaningful time relevant to the problem
being solved. The second element is the <em>logical</em> time, and it orders actions that occur too
quickly to be assigned distinct <em>real</em> times. Simulation time is ordered lexicographiclly.
Specifically,</P> <center> <P>(t,c) &lt; (h,k) iff t &lt; h or (t=h and c &lt; k).</P>
</center>
<P>Time is advanced by an operator indicated in the simulation code with +, but that does not
coincide with the normal notion of addition. The + operator is defined to be</P> <center>
<P>(t,c)+(h,k) = if h &ne; 0 then (t+h,k) else (t,c+k).</P>
</center>
<P>The <em>next instant</em> of simulation time following (t,c) is (t,c)+(0,1)=(t,c+1). Hence, if a
message is sent at time (t,c) it will be delivered at (t,c+1). Likewise, if the update() or init()
method is invoked at (t,c) it must return a time greater than or equal to (t,c+1). A model removed
at (t,c) has its fini() method called at (t,c+1).</P> <P>Given two time points (t,c) &lt; (h,k) the
interval [(t,c),(h,k)) has a length equal to<P> <center> <P>(h,k)-(t,c) = if h = t then (0,k-c) else
(h-t,k).</P>
</center>
<P>This is used to calculate the elapsed time for a Parallel DEVS model.</P>
<P>Sample simulation programs can be found in the examples directory. These, in conjunction with the
information above and possibly a book on Parallel DEVS will be the best way to get started building
simulation programs. Here is a list of examples that are available to get you started.</P> <UL>
<LI><P><a href="../examples/simple.cpp">simple.cpp</a>: A trivial example of a single person who
advances through the stages of Life. It is amongst the smallest simulation programs that you can
build.
</P></LI>
<LI><P><a href="../examples/queue.cpp">queue.cpp</a>: A queuing system implemented using the
messaging api.
</P></LI>
<LI><P><a href="../examples/glife.cpp">glife.cpp</a>: The Game of Life implemented using Parallel
DEVS.
</P></LI>
<LI><P><a href="../examples/glife2.cpp">glife2.cpp</a>: The Game of Life implemented using the
message passing api with integers for time instead of the pair (t,c).
</P></LI>
</UL>

 */

#include "adevs_base.h"
#include "adevs_pdevs.h"

#endif
