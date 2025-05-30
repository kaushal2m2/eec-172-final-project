import React from 'react';
import '../styles/About.css';
import picture1 from '../assets/picture1.png';
import picture2 from '../assets/picture2.png';
import picture3 from '../assets/picture3.png';
import logo from '../assets/logo.png';

export default function About() {
  return (
    <div className="about-container">
      <div className="about-header">
        <div className="header-content">
          <h1 className="about-title">TI-OS™ Workstation</h1>
          <p className="about-intro">
            TI-OS™ Workstation is the most powerful desktop operating system 
            for your most demanding business needs. Run high-end, 32-bit applications
            for software development, engineering, financial analysis, scientific, 
            and business-critical tasks along with other TI-OS™-based software, all in
            a secure, multitasking environment.
          </p>
        </div>
      </div>

      <div className="about-grid">
        <div className="grid-item feature-left">
          <img src={picture1} alt="Feature 1" className="feature-image" />
          <div className="feature-caption">
            <h4>Edit in place, and even drag and drop</h4>
            <p>Data between 16-bit and 32-bit applications, using OLE, TI's object technology.</p>
          </div>
        </div>

        <div className="grid-item feature-right">
          <img src={picture2} alt="Feature 2" className="feature-image" />
          <div className="feature-caption">
            <h4>High-end feature support</h4>
            <p>Makes it easier than ever to deliver workstation or mini-computer performance on the more affordable PC platform.</p>
          </div>
        </div>

        <div className="grid-item specs-box">
          <h3>System requirements:</h3>
          <ul className="specs-list">
            <li>x86 or Pentium-based system</li>
            <li>Personal computer with a 386/25 processor or higher</li>
            <li>12 MB of memory</li>
            <li>One CD-ROM drive and a hard disk with 90 MB available</li>
            <li>VGA or higher-resolution video display adapter</li>
          </ul>
          <h3>Networking options:</h3>
          <ul className="specs-list">
            <li>Ethernet</li>
            <li>TCP/IP</li>
            <li>Wi-Fi</li>
            <li>Bluetooth</li>
          </ul>
        </div>

        <div className="grid-item feature-large">
          <img src={picture3} alt="Feature 3" className="feature-image-large" />
          <div className="feature-caption-large">
            <h4>Build custom applications right on target.</h4>
            <p>TI-OS™ Workstation multiplexes applications efficiently in their own memory
            areas, making it an excellent host and target for custom development. And with 
            support for advanced features such as OLE, OpenGL graphics, multithreading, 
            and symmetric multiprocessing, never has there been so much power readily 
            accessible for everyday use.</p>
          </div>
        </div>
      </div>
      
      <div className="about-footer">
        <div className="footer-box">
          <h3>Expand tomorrow's choices with today's flexible solutions.</h3>
          <ul>
            <li>Extend your current investment, training, and expertise in the operating system as you move to a more powerful computing platform.</li>
            <li>Preemptively multitask 16-bit and 32-bit applications freely in a secure, robust environment.</li>
            <li>Add more processors to current PCs as your needs grow, or even explore the latest RISC processors, while keeping your existing applications.</li>
          </ul>
        </div>
        <div className="footer-logo">
          <img src={logo} alt="TI-OS Logo" className="logo-image" />
          <p className="trademark">© 2023 TI-OS Corporation. All rights reserved.</p>
        </div>
      </div>
    </div>
  );
}
