---
layout: post
title: "Apache Tika VS Boredom. Bypassing Arbitrary File Upload Restrictions"
date: 2020-07-15 09:00:00 +0200
tags: [Apache, Java, Tika, Arbitrary File Upload, Bypass]
author: MrI/O
categories: [research, clients]
excerpt_separator: <!--more-->

---

As we were preparing some stuff for one of our clients, they requested for us to provide some insight in how to avoid arbitrary file uploads.

Apart of the usual _"just use a white-list for the allowed extensions"_ approach, he wanted us to focus on analyzing the uploaded files to be able to determine whether if these should be allowed or not. 

<!--more-->

To achieve it, they suggested an Apache library called [Tika](https://tika.apache.org/).

_Apache Tika(TM) is a toolkit for detecting and extracting metadata and structured text content from various documents using existing parser libraries._ 
> Taken from its [GitHub repo](https://github.com/apache/tika).


It seems like a robust method to perform file parsing and metadata detection, however, when used as a file type detection mechanism, is easy to make it fail miserably. 


Let's take a look.


### Setting up the environment

It's fairly easy to get it running, just clone the repo and let Maven do its thing.

{% highlight bash linenos %}

git clone https://github.com/apache/tika.git tika-trunk
cd tika-trunk
mvn install -DskipTests
{% endhighlight %} 

Go get a coffee, Maven is gonna take some time fetching the dependencies and building your JAR... In fact, grow your own coffee, recollect it, toast, grind and brew it yourself. Maybe once you're done Maven is finished as well.


Now change to the JAR file folder and execute the Tika server: 


{% highlight bash linenos %}

cd tika-server/target
java -jar tika-server-2.0.0-SNAPSHOT.jar
{% endhighlight %}

> Note: The tika-server version may vary on your environment.


If successful, you should see something like this: 

![Tika up and serving!](/assets/clients/tika/tika1.png)

Good, so it's up and serving at [localhost:9998](http://localhost:9998). Heading to that direction shows the Apache Tika Server endpoints with all the supported methods and a brief description of what to expect from these. 

![Tika server endpoints!](/assets/clients/tika/tika2.png)


### Giving Tika a try


The first endpoint for the Tika Server API is the one we're interested on: `/detect/stream`

It allows to `PUT` a file and it should _detect_ the kind of file it is receiving. Fairly simple and straightforward. 

All right, let's try it.

{% highlight bash linenos %}

curl -T some_random_img.png http://localhost:9998/detect/stream -v
{% endhighlight %} 

Which should return something like this:

![Tika server file detection!](/assets/clients/tika/tika3.png)


Nice and simple! Tika detected the file we uploaded as an PNG image. So far so good.

But, is this always right? Is Tika always correct when detecting the file type?


### Tika take this

A common way to bypass upload filters that rely on the so-called Magic Bytes is adding the typical `GIF87a;` or `GIF89a;` header to the restricted file to upload. 

Tika is no different from other tools, and will mess it up with the types as soon as those bytes are added to the file: 


![Tika server file detection mismatch!](/assets/clients/tika/tika4.png)

However, Tika is not yet defeated. There's another endpoint that we should be able to bypass.


#### /tika to the rescue

Attempting to `PUT` that very same file to the `/tika` endpoint will return a fairly different result: 


![Tika server file detection mismatch!](/assets/clients/tika/tika5.png)

Humm... no like?

It seems the `/tika` endpoint tries to parse the whole file, and realizes it is something weird. 

To bypass it, by prepending a whole GIF before the code, would do the trick: 

![Full GIF before code!](/assets/clients/tika/tika6.png)

And then attempting to `PUT` it against both endpoints: 

![Bypassed!](/assets/clients/tika/tika7.png)


Nice! Tika's checks were bypassed :)

---

### Conclusions


Sometimes, the big names in the game give us a false sense of security.

For sure Apache Tika is a great utility to perform metadata analysis and, in some environments, file detection. However, when it comes to security, it shouldn't be relied on as the only method to avoid unrestricted file uploads. 


This, rather simplistic, and small article is the result of a few minutes of research taken in order to better advice one of our clients.



#### References

- [Apache Tika](https://tika.apache.org/)
- [Apache Tika GitHub](https://github.com/apache/tika)
- [Apache Maven](https://maven.apache.org/)
- [Magic Bytes](https://en.wikipedia.org/wiki/List_of_file_signatures)




































































