#include <gio/gio.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <grp.h>
#include <hybris/properties/properties.h>

#define CONFIGFS "/sys/kernel/config"
#define CONFIGDIR CONFIGFS "/usb_gadget"
#define GADGETDIR CONFIGDIR "/g1"
#define CONFIGNAME "c.1"
#define RNDISCONFIG "rndis.usb0"
#define MTPCONFIG "mtp.gs0"

#define IDVENDOR "0x2717"
#define IDPRODUCT "0xFF20"
#define BCDDEVICE "0x0223"
#define BCDUSB "0x0200"

static const gchar introspection_xml[] =
  "<node>"
  "  <interface name='io.FuriOS.USBConfig'>"
  "    <method name='SetUSBMode'>"
  "      <arg type='s' name='mode' direction='in'/>"
  "    </method>"
  "  </interface>"
  "</node>";

static void
write_to_file (const char *path,
               const char *value)
{
  g_print ("Attempting to write to %s: %s\n", path, value);
  int fd = open (path, O_WRONLY);
  if (fd == -1) {
    perror ("open");
    return;
  }

  if (write (fd, value, strlen (value)) == -1)
    perror ("write");

  close (fd);
}

static void
cleanup_configfs ()
{
  unlink (GADGETDIR "/configs/" CONFIGNAME "/" MTPCONFIG);
  unlink (GADGETDIR "/configs/" CONFIGNAME "/" RNDISCONFIG);
  unlink (GADGETDIR "/configs/" CONFIGNAME "/rndis.usb0");
  unlink (GADGETDIR "/configs/" CONFIGNAME "/rndis_bam.rndis");
  unlink (GADGETDIR "/configs/" CONFIGNAME "/rndis.0");
}

static void
configure_mtp ()
{
  g_print ("Configuring for mode MTP\n");

  // Mount configfs if not already mounted
  if (access (CONFIGFS, F_OK) == -1) {
    if (mount ("none", CONFIGFS, "configfs", 0, NULL) == -1) {
      perror ("mount");
      return;
    }
  }

  mkdir (GADGETDIR "/strings/0x409", 0755);
  mkdir (GADGETDIR "/functions/" RNDISCONFIG, 0755);
  mkdir (GADGETDIR "/functions/rndis.usb0", 0755);
  mkdir (GADGETDIR "/functions/rndis_bam.rndis", 0755);
  mkdir (GADGETDIR "/configs/" CONFIGNAME "/strings/0x409", 0755);

  write_to_file (GADGETDIR "/idVendor", IDVENDOR);
  write_to_file (GADGETDIR "/idProduct", IDPRODUCT);
  write_to_file (GADGETDIR "/bcdDevice", BCDDEVICE);
  write_to_file (GADGETDIR "/bcdUSB", BCDUSB);
  write_to_file (GADGETDIR "/os_desc/use", "1");
  write_to_file (GADGETDIR "/os_desc/b_vendor_code", "0x1");
  write_to_file (GADGETDIR "/os_desc/qw_sign", "MSFT100");

  char serialnumber[PROP_VALUE_MAX];
  char manufacturer[PROP_VALUE_MAX];
  char product[PROP_VALUE_MAX];
  char controller[PROP_VALUE_MAX];

  property_get ("ro.serialno", serialnumber, "");
  property_get ("ro.product.vendor.manufacturer", manufacturer, "");
  property_get ("ro.product.vendor.model", product, "");
  property_get ("sys.usb.controller", controller, "");

  write_to_file (GADGETDIR "/strings/0x409/serialnumber", serialnumber);
  write_to_file (GADGETDIR "/strings/0x409/manufacturer", manufacturer);
  write_to_file (GADGETDIR "/strings/0x409/product", product);

  mkdir (GADGETDIR "/functions/" MTPCONFIG, 0755);
  symlink (GADGETDIR "/configs/" CONFIGNAME, GADGETDIR "/os_desc/" CONFIGNAME);

  chown (GADGETDIR, 0, getgrnam ("plugdev")->gr_gid);
  chown (GADGETDIR "/configs", 0, getgrnam ("plugdev")->gr_gid);
  chown (GADGETDIR "/configs/" CONFIGNAME, 0, getgrnam ("plugdev")->gr_gid);
  chown ("/dev/mtp_usb", 0, getgrnam ("plugdev")->gr_gid);
  chmod ("/dev/mtp_usb", 0660);

  cleanup_configfs ();

  write_to_file (GADGETDIR "/functions/" MTPCONFIG "/os_desc/interface.MTP/compatible_id", "mtp");
  write_to_file (GADGETDIR "/configs/" CONFIGNAME "/strings/0x409/configuration", "mtp");

  symlink (GADGETDIR "/functions/" MTPCONFIG, GADGETDIR "/configs/" CONFIGNAME "/" MTPCONFIG);

  write_to_file (GADGETDIR "/os_desc/use", "1");
  write_to_file (GADGETDIR "/UDC", controller);
}

static void
configure_rndis ()
{
  g_print ("Configuring for mode RNDIS\n");

  cleanup_configfs ();

  mkdir (GADGETDIR "/functions/" RNDISCONFIG, 0755);

  write_to_file (GADGETDIR "/idVendor", IDVENDOR);
  write_to_file (GADGETDIR "/idProduct", IDPRODUCT);
  write_to_file (GADGETDIR "/bcdDevice", BCDDEVICE);
  write_to_file (GADGETDIR "/bcdUSB", BCDUSB);

  mkdir (GADGETDIR "/configs/" CONFIGNAME, 0755);
  mkdir (GADGETDIR "/configs/" CONFIGNAME "/strings/0x409", 0755);
  write_to_file (GADGETDIR "/configs/" CONFIGNAME "/strings/0x409/configuration", "RNDIS");

  symlink (GADGETDIR "/functions/" RNDISCONFIG, GADGETDIR "/configs/" CONFIGNAME "/" RNDISCONFIG);

  char serialnumber[PROP_VALUE_MAX];
  char manufacturer[PROP_VALUE_MAX];
  char product[PROP_VALUE_MAX];
  char controller[PROP_VALUE_MAX];

  property_get ("ro.serialno", serialnumber, "");
  property_get ("ro.product.vendor.manufacturer", manufacturer, "");
  property_get ("ro.product.vendor.model", product, "");
  property_get ("sys.usb.controller", controller, "");

  write_to_file (GADGETDIR "/strings/0x409/serialnumber", serialnumber);
  write_to_file (GADGETDIR "/strings/0x409/manufacturer", manufacturer);
  write_to_file (GADGETDIR "/strings/0x409/product", product);
  write_to_file (GADGETDIR "/UDC", controller);
}

static void
configure_none ()
{
  g_print ("Configuring for mode NONE\n");

  cleanup_configfs ();

  write_to_file (GADGETDIR "/UDC", "");
}

static void
handle_method_call (GDBusConnection *connection, const gchar *sender, const gchar *object_path,
                    const gchar *interface_name, const gchar *method_name, GVariant *parameters,
                    GDBusMethodInvocation *invocation, gpointer user_data)
{
  gchar *mode;
  g_variant_get (parameters, "(&s)", &mode);

  if (g_strcmp0 (method_name, "SetUSBMode") == 0) {
    if (g_strcmp0 (mode, "mtp") == 0)
      configure_mtp ();
    else if (g_strcmp0 (mode, "rndis") == 0)
      configure_rndis();
    else if (g_strcmp0 (mode, "none") == 0)
      configure_none ();
  }

  g_dbus_method_invocation_return_value (invocation, NULL);
}

static const
GDBusInterfaceVTable interface_vtable = {
  handle_method_call,
  NULL,
  NULL};

static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar *name, gpointer user_data)
{
  GDBusNodeInfo *introspection_data = (GDBusNodeInfo *) user_data;

  GError *error = NULL;

  g_dbus_connection_register_object(
    connection,
    "/io/FuriOS/USBConfig",
    introspection_data->interfaces[0],
    &interface_vtable,
    NULL,
    NULL,
    &error);

  if (error) {
    g_printerr ("Error registering object: %s\n", error->message);
    g_error_free (error);
  }
}

static void
on_name_acquired (GDBusConnection *connection,
                  const gchar *name, gpointer user_data)
{
  g_print ("Name acquired: %s\n", name);
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar *name, gpointer user_data)
{
  g_printerr ("Name lost: %s\n", name);
}

int
main (int argc, char *argv[])
{
  GMainLoop *loop;
  guint owner_id;
  GError *error = NULL;

  GDBusNodeInfo *introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, &error);
  if (error) {
    g_printerr ("Error parsing introspection XML: %s\n", error->message);
    g_error_free (error);
    return 1;
  }

  owner_id = g_bus_own_name(
    G_BUS_TYPE_SYSTEM,
    "io.FuriOS.USBConfig",
    G_BUS_NAME_OWNER_FLAGS_NONE,
    on_bus_acquired,
    on_name_acquired,
    on_name_lost,
    introspection_data,
    NULL);

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  g_bus_unown_name (owner_id);
  g_dbus_node_info_unref (introspection_data);
  g_main_loop_unref (loop);

  return 0;
}
